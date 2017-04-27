#ifndef _TCP_INTERNAL_H_
#define _TCP_INTERNAL_H_

#include <condition_variable>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "tcp-buffer.h"
#include "tcp-state-machine.h"

namespace tcp_simulator {

class TcpSocket;
class TcpManager;

class TcpInternal : public TcpInternalInterface {
 public:
  friend class TcpManager;
  
  TcpInternal(uint64_t id, TcpManager &manager, uint16_t host_port,
              uint16_t peer_port);
  
  TcpInternal(const TcpInternal &) = delete;
  TcpInternal(TcpInternal &&) = delete;
  
  TcpInternal &operator=(const TcpInternal &) = delete;
  TcpInternal &operator=(TcpInternal &&) = delete;
  
  ~TcpInternal();
  
  // Api for Socket
  TcpSocket SocketAcceptConnection();

  int SocketListen(uint16_t port) {
    std::lock_guard<TcpManager> guard(tcp_manager_);
    return Listen(port, guard);
  }
  void SocketConnect(uint16_t port, uint32_t seq, uint16_t window) {
    {
      std::lock_guard<TcpManager> guard(tcp_manager_);
      Connect(port, seq, window, guard);
    }
    
    std::unique_lock<TcpManager> lock(tcp_manager_);
    connected_.first.wait(lock, [this](){return connected_.second;});
  }
  
  void SocketCloseConnection() {
    std::lock_guard<TcpManager> guard(tcp_manager_);
    CloseConnection();
  }
  
  std::list<TcpPacket> SocketGetReceivedPackets() {
    std::unique_lock<TcpManager> lock(tcp_manager_);
    data_for_reading_.first.wait(lock, [this](){
          return data_for_reading_.second > 0;
        });
    lock.unlock();
    
    std::lock_guard<TcpManager> guard(tcp_manager_);
    data_for_reading_.second = 0;
    return buffer_.GetReadPackets();
  }
  
  int SocketAddPacketForSending(const char *begin, const char *end) {
    std::lock_guard<TcpManager> guard(tcp_manager_);
    return AddPacketForSending(TcpPacket(begin, end));
  }
  
 private:
  // Api for TcpManager/StateMachine
  std::list<std::shared_ptr<NetworkPacket> > GetPacketsForSending();
  std::list<std::shared_ptr<NetworkPacket> > GetPacketsForResending();
  
  void ReceivePacket(TcpPacket packet);
  
  auto HostPort() {
    return host_port_;
  }
  
  auto PeerPort() {
    return peer_port_;
  }
  
  auto GetRstPacket() {
    TcpPacket packet(nullptr, nullptr);
    packet.GetHeader().SetRst(true);
    packet.GetHeader().SetAck(true);

    return packet;
  }
  
  State GetState() {
    return state_;
  }
  
  auto Id() const {
    return id_;
  }
  
  // misc
  void Connect(uint16_t port, uint32_t seq, uint16_t window, 
      const std::lock_guard<TcpManager> &guard);
  
  int Listen(uint16_t port, const std::lock_guard<TcpManager> &guard);
  
  int AddPacketForSending(TcpPacket packet);

  void CloseConnection() {
    auto result = state_.FinSent();
    assert(result);
    SendFin();
    unsequenced_packets_.clear();
  }
  
  // callback action
  void SendAck() override {
    std::cerr << __func__ << std::endl;
    TcpPacket packet(nullptr, nullptr);
    packet.GetHeader().SetAck(true);

    AddPacketForSending(std::move(packet));
  }
  
  void SendConditionAck() override {
    std::cerr << __func__ << std::endl;
    if (state_.GetLastPacketSize() == 0)
      return ;
    
    TcpPacket packet(nullptr, nullptr);
    packet.GetHeader().SetAck(true);

    AddPacketForSending(std::move(packet));
  }
  
  void SendSyn() override {
    std::cerr << __func__ << std::endl;
    TcpPacket packet(nullptr, nullptr);
    packet.GetHeader().SetSyn(true);

    AddPacketForSending(std::move(packet));
  }
  void SendSynAck() override {
    std::cerr << __func__ << std::endl;
    TcpPacket packet(nullptr, nullptr);
    packet.GetHeader().SetSyn(true);
    packet.GetHeader().SetAck(true);

    AddPacketForSending(std::move(packet));
  }
  
  void SendFin() override {
    std::cerr << __func__ << std::endl;
    TcpPacket packet(nullptr, nullptr);
    packet.GetHeader().SetFin(true);
    packet.GetHeader().SetAck(true);

    AddPacketForSending(std::move(packet));
  }
  
  void SendRst() override {
    std::cerr << __func__ << std::endl;
    TcpPacket packet(nullptr, nullptr);
    packet.GetHeader().SetRst(true);
    packet.GetHeader().SetAck(true);

    AddPacketForSending(std::move(packet));
  }
  
  void Accept() override {
    std::cerr << __func__ << std::endl;
    if(state_ != State::kEstab || current_packet_.Length() == 0)
      return ;
    
    std::cerr << "Adding packet to unsequenced" << std::endl;
    unsequenced_packets_.emplace(
        current_packet_.GetHeader().SequenceNumber(),
        std::move(current_packet_));
  }
  
  void Discard() override {
    std::cerr << __func__ << std::endl;
  }
  
  void Close() override {
    std::cerr << __func__ << std::endl;
    Reset();
  }
  
  void NewConnection() override;
  
  void Reset() override;
  
  void NotifyOnConnected() override {
    connected_.second = true;
    connected_.first.notify_one();
  }
  
  void NotifyOnReceivingMessage() override {
    ++data_for_reading_.second;
    data_for_reading_.first.notify_one();
  }
  
  void NotifyOnNewConnection(size_t count) override {
    new_connection_.second = count;
    new_connection_.first.notify_one();
  }

  uint64_t id_;
  
  TcpBuffer buffer_;
  TcpStateMachine state_;
  
  TcpManager &tcp_manager_;
  
  TcpPacket current_packet_;

  uint16_t host_port_;
  uint16_t peer_port_;
  
  std::map<uint32_t, TcpPacket> unsequenced_packets_;
  
  // notification
  std::pair<std::condition_variable_any, bool> connected_;
  std::pair<std::condition_variable_any, size_t> data_for_reading_;
  std::pair<std::condition_variable_any, size_t> new_connection_;
};

} // namespace tcp_simulator

#endif // _TCP_INTERNAL_H_
