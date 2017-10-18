#ifndef _TCP_STACK_SOCKET_INTERNAL_H_
#define _TCP_STACK_SOCKET_INTERNAL_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <tuple>

#include "safe-log.h"
#include "state.h"
#include "tcp-buffer.h"

namespace tcp_stack {

inline void SynHeader(uint32_t seq, uint16_t window, TcpHeader *header) {
  header->SetSyn(true);

  header->SequenceNumber() = seq;
  header->Window() = window;
}

inline void AckHeader(uint32_t seq, uint32_t ack, uint16_t window, TcpHeader *header) {
  header->SetAck(true);

  header->SequenceNumber() = seq;
  header->AcknowledgementNumber() = ack;
  header->Window() = window;
}

inline void SynAckHeader(uint32_t seq, uint32_t ack, uint16_t window,
                  TcpHeader *header) {
  SynHeader(seq, window, header);
  AckHeader(seq, ack, window, header);
}

inline void FinHeader(uint32_t seq, uint32_t ack, uint16_t window, TcpHeader *header) {
  header->SetFin(true);
  AckHeader(seq, ack, window, header);
}

inline void RstHeader(uint32_t seq, TcpHeader *header) {
  header->SetRst(true);
  header->SequenceNumber() = seq;
}

inline void RstHeader(const TcpHeader &source_header, TcpHeader *header) {
  RstHeader(source_header.AcknowledgementNumber(), header);
}

inline void SetSource(uint32_t ip, uint16_t port, TcpHeader *header) {
  header->SourceAddress() = ip;
  header->SourcePort() = port;
}

inline void SetDestination(uint32_t ip, uint16_t port, TcpHeader *header) {
  header->DestinationAddress() = ip;
  header->DestinationPort() = port;
}

struct SocketIdentifier {
  friend struct std::hash<SocketIdentifier>;

  friend bool operator==(const SocketIdentifier &lhs,
                         const SocketIdentifier &rhs) {
    return lhs.host_ip_ == rhs.host_ip_ && lhs.host_port_ == rhs.host_port_ &&
        lhs.peer_ip_ == rhs.peer_ip_ && lhs.peer_port_ == rhs.peer_port_;
  }
      
  SocketIdentifier(uint32_t host_ip, uint16_t host_port, uint32_t peer_ip,
      uint16_t peer_port)
      : host_ip_(host_ip), host_port_(host_port), peer_ip_(peer_ip),
        peer_port_(peer_port) {}
      
  SocketIdentifier(const TcpHeader &header)
      : host_ip_(header.DestinationAddress()),
        host_port_(header.DestinationPort()),
        peer_ip_(header.SourceAddress()),
        peer_port_(header.SourcePort()) {}
  
  void SetHostPort(uint16_t port) {
    host_port_ = port;
  }

private:
  uint32_t host_ip_;
  uint16_t host_port_;

  uint32_t peer_ip_;
  uint16_t peer_port_;
};

} // namespace tcp_stack

namespace std {
template <>
struct hash<tcp_stack::SocketIdentifier> {
  size_t operator()(const tcp_stack::SocketIdentifier &x) const {
    uint32_t result = x.host_ip_;
    result = result*2654435761 + x.host_port_;
    result = result*2654435761 + x.peer_ip_;
    return result*2654435761 + x.peer_port_;
  }
};

} // namespace std

namespace tcp_stack {
template <class... Lockable>
class LockGuard {
public:
  LockGuard(Lockable&... locks) : locks_(locks...) {
    (locks.lock(), ...);
  }

  LockGuard(const LockGuard &) = delete;

  ~LockGuard() {
    UnLock(std::index_sequence_for<Lockable...>());
  }

  LockGuard &operator=(const LockGuard &) = delete;

private:
  template <size_t... indexs>
  void UnLock(std::index_sequence<indexs...>) {
    (std::get<indexs>(locks_).unlock(), ...);
  }

  std::tuple<Lockable&...> locks_;
};

class SocketManager;

class SocketInternal : private SocketInternalInterface,
                       public std::enable_shared_from_this<SocketInternal> {
public:
  SocketInternal(std::shared_ptr<TcpPacket> packet, SocketManager *manager)
      : host_ip_(packet->GetHeader().DestinationAddress()),
        host_port_(packet->GetHeader().DestinationPort()),
        peer_ip_(packet->GetHeader().SourceAddress()),
        peer_port_(packet->GetHeader().SourcePort()),
        manager_(manager) {
    Log("SocketInternal from packet");
    RecvPacket(std::move(packet), true);
  }

  SocketInternal(uint32_t host_ip, uint16_t host_port, SocketManager *manager)
      : host_ip_(host_ip), host_port_(host_port), manager_(manager) {}
  
  SocketInternal(const SocketInternal &) = delete;

  ~SocketInternal() {
    Log(__func__);
  }

  SocketInternal &operator=(const SocketInternal &) = delete;

  // API for SocketManager
  void RecvPacket(std::shared_ptr<TcpPacket> packet, bool check_sum_validate) {
    if (!check_sum_validate) {
      Log("Invalide Checksum");
      state_.InvalideCheckSum()(this);
    } else {
      std::lock_guard guard(*this);
      Log("RecvPacket");
      current_packet_ = packet;
      state_(packet->GetHeader())(this);
    }
  }

  bool IsClosed() {
    std::lock_guard guard(*this);
    return state_.GetState() == State::kClosed;
  }

  void SignalANewConnection() {
    wait_until_readable_.notify_all();
  }

  bool IsAnyPacketForSending(const std::lock_guard<SocketInternal> &) {
    return !send_buffer_.Empty();
  }

  std::shared_ptr<TcpPacket> GetPacketForSending(
      const std::lock_guard<SocketInternal> &) {
    // TODO: rewrite sending buffer
    if (send_buffer_.Empty())
      return {};
    
    auto packet = send_buffer_.GetAsTcpPacket(0, state_.Window());
    
    state_(Event::kSend, &packet->GetHeader())(this);
    SetSource(host_ip_, host_port_, &packet->GetHeader());
    SetDestination(peer_ip_, peer_port_, &packet->GetHeader());
    
    TcpHeaderH2N(packet->GetHeader());
    return packet;
  }

  void Reset() {
    std::lock_guard guard(*this);

    host_ip_ = 0;
    host_port_ = 0;

    peer_ip_ = 0;
    peer_port_ = 0;

    send_buffer_.Clear();
    recv_buffer_.clear();
    
    state_.Reset();
  }

  // API for TcpSocket
  void SocketListen(uint16_t port) {
    std::lock_guard lck(*this);
    next_host_port_ = port;

    state_(Event::kListen, nullptr)(this);
  }

  void SocketConnect(uint32_t ip, uint16_t port) {
    next_peer_ip_ = ip;
    next_peer_port_ = port;

    auto lck = SelfUniqueLock();
    state_(Event::kConnect, nullptr)(this);

    wait_until_readable_.wait(lck,
        [this]() {return state_.GetState() == State::kEstab;});
  }

  std::shared_ptr<SocketInternal> SocketAccept();

  void SocketSend(const char *first, size_t size);

  size_t SocketRecv(char *first, size_t size) {
    std::unique_lock lck(mtx_);
    
    bytes_demand_.store(size);
    wait_until_readable_.wait(lck, [this, size] {
          return recv_buffer_.size() >= size;
        });
      
    const auto source_first = recv_buffer_.begin();
    const auto source_last =
        recv_buffer_.begin() + std::min(size, recv_buffer_.size());
    std::copy(source_first, source_last, first);
    recv_buffer_.erase(source_first, source_last);
    bytes_demand_.store(0);
    return 0;
  }

  void SocketClose() {
    std::lock_guard lck(*this);
    state_(Event::kClose, nullptr)(this);
  }

  void SocketDestroyed();

  void lock() {
    mtx_.lock();
  }

  bool try_lock() {
    return mtx_.try_lock();
  }

  void unlock() {
    mtx_.unlock();
  }

  std::unique_lock<std::mutex> SelfUniqueLock() {
    return std::unique_lock(mtx_);
  }

private:
  void SendPacket(std::shared_ptr<TcpPacket> packet);
  void SendPacketWithResend(std::shared_ptr<TcpPacket> packet);

  void SendSyn(uint32_t seq, uint16_t window) override;

  void SendSynAck(uint32_t seq, uint32_t ack, uint16_t window) override {
    auto packet = MakeTcpPacket(0);
    SynAckHeader(seq, ack, window, &packet->GetHeader());
    send_buffer_.InitializeAckNumber(seq + 1);

    SendPacketWithResend(std::move(packet));
  }

  void SendAck(uint32_t seq, uint32_t ack, uint16_t window) override {
    auto packet = MakeTcpPacket(0);
    AckHeader(seq, ack, window, &packet->GetHeader());

    Log("Ack", packet->GetHeader().TcpLength());
    SendPacket(std::move(packet));
  }

  void SendFin(uint32_t seq, uint32_t ack, uint16_t window) override {
    auto packet = MakeTcpPacket(0);
    FinHeader(seq, ack, window, &packet->GetHeader());
    SendPacketWithResend(std::move(packet));
  }

  void RecvSyn(uint32_t seq_recv, uint16_t window_recv) override {}

  void RecvAck(
      uint32_t seq_recv, uint32_t ack_recv, uint16_t window_recv) override {
    send_buffer_.Ack(ack_recv);
  }

  void RecvFin(
      uint32_t seq_recv, uint32_t ack_recv, uint16_t window_recv) override {}
  
  void Listen() override;

  void Connected() override {
    wait_until_readable_.notify_all();
  }

  void Accept() override {
    Log(__func__);
    auto packet = current_packet_.lock();
    if (packet->GetHeader().TcpLength() > 0) {
      
      recv_buffer_.insert(recv_buffer_.end(), packet->begin(), packet->end());
      Log("With Content");
      if (bytes_demand_.load() != 0 &&
          recv_buffer_.size() >= bytes_demand_.load())
        wait_until_readable_.notify_all();
    }
  }

  void Discard() override {
    Log(__func__);
  }

  void SeqOutofRange(uint16_t window) override {
    assert(false);
  }

  void SendRst(uint32_t seq) override {
    auto packet = MakeTcpPacket(0);
    RstHeader(seq, &packet->GetHeader());
    SendPacket(std::move(packet));
  }

  void InvalidOperation() override {
    throw std::runtime_error(__func__);
  }

  void NewConnection() override;

  void Close() override;
  void TimeWait() override;

  SocketIdentifier GetIdentifier() const {
    return SocketIdentifier(host_ip_, host_port_, peer_ip_, peer_port_);
  }

  uint32_t host_ip_ = 0;
  uint16_t host_port_ = 0;

  uint32_t peer_ip_ = 0;
  uint16_t peer_port_ = 0;

  uint16_t next_host_port_ = 0;

  uint32_t next_peer_ip_ = 0;
  uint16_t next_peer_port_ = 0;

  std::weak_ptr<TcpPacket> current_packet_;

  TcpSendingBuffer send_buffer_;
  std::deque<char> recv_buffer_;
  TcpStateManager state_;

  SocketManager * const manager_;

  std::mutex mtx_;

  // for blocked recv
  std::atomic<size_t> bytes_demand_{0};
  std::condition_variable wait_until_readable_;
};

} // namespace tcp_stack

#endif // _TCP_STACK_SOCKET_INTERNAL_H_
