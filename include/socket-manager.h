#ifndef _TCP_STACK_SOCKET_MANAGER_H_
#define _TCP_STACK_SOCKET_MANAGER_H_

#include <chrono>
#include <list>
#include <mutex>
#include <numeric>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "socket-internal.h"
#include "tcp-socket.h"
#include "timeout-queue.h"

namespace tcp_stack {
class SocketManager {
public:
  SocketManager(uint32_t ip) {
    timeout_queue_.AsyncRun();
    ip_ = ip;
  }

  ~SocketManager() {
    std::cout << __func__ << std::endl;
  }

  template <class Predicate>
  void InternalSendPacketWithResend(std::shared_ptr<TcpPacket> packet,
                                    Predicate pred) {
    std::cout << __func__ << std::endl;
    constexpr auto resent_timeout = std::chrono::seconds(5);
    SendPacket(packet);
    timeout_queue_.PushEvent(
        [packet = std::move(packet), pred = std::move(pred), this]() {
          const bool is_valid = pred(packet);
          std::cout << "Time out" << is_valid << std::endl;
          if (is_valid)
            SendPacket(packet);
          return is_valid;
        }, resent_timeout);
  }

  void InternalSendPacket(std::shared_ptr<TcpPacket> packet) {
    std::cout << __func__ << std::endl;
    SendPacket(packet);
  }

  void InternalListen(std::shared_ptr<SocketInternal> internal,
                      const SocketIdentifier &id) {
    std::lock_guard guard(*this);
    assert(unused_sockets_.find(internal) != unused_sockets_.end());
    
    unused_sockets_.erase(internal);

    if (identifier_to_socket_.find(id) != identifier_to_socket_.end())
      throw std::runtime_error("port used.");
    identifier_to_socket_.emplace(id, std::move(internal));
  }

  void InternalNewConnection(SocketInternal *internal,
                             std::shared_ptr<TcpPacket> packet) {
    SocketIdentifier id(packet->GetHeader());
    auto new_socket = std::make_shared<SocketInternal>(std::move(packet), this);

    std::lock_guard guard(*this);

    if (new_socket->IsClosed())
      return ;

    auto new_connection_list = new_connections_.find(internal);
    if (new_connection_list == new_connections_.end())
      new_connections_[internal].emplace_back(new_socket);
    else
      new_connection_list->second.emplace_back(new_socket);

    assert(identifier_to_socket_.find(id) == identifier_to_socket_.end());
    identifier_to_socket_.emplace(id, new_socket);

    internal->SignalANewConnection();
  }

  void InternalConnectTo(
      std::shared_ptr<SocketInternal> internal,
      uint16_t host_port, uint32_t peer_ip, uint16_t peer_port) {
    SocketIdentifier id(ip_, host_port, peer_ip, peer_port);

    std::lock_guard guard(*this);
    unused_sockets_.erase(internal);
    identifier_to_socket_[id] = internal;
  }

  auto SelfUniqueLock() {
    return std::unique_lock(mtx_);
  }

  bool InternalAnyNewConnection(SocketInternal *internal) {
    auto ite = new_connections_.find(internal);
    if (ite == new_connections_.end() || ite->second.empty())
      return false;
    return true;
  }

  std::shared_ptr<SocketInternal> InternalGetNewConnection(
      SocketInternal *internal, const SocketIdentifier &id) {
    auto ite = new_connections_.find(internal);
    if (ite == new_connections_.end() || ite->second.empty())
      return {};
    
    auto connection = ite->second.front();
    ite->second.pop_front();

    identifier_to_socket_[id] = connection;

    return connection;
  }

  void InternalHasPacketForSending(std::shared_ptr<SocketInternal> internal) {
    std::lock_guard guard(*this);
    sockets_wait_for_sending_.insert(std::move(internal));
  }

  // Called when TcpSocket is destroyed
  void InternalClosing(const std::shared_ptr<SocketInternal> &internal) {
    std::lock_guard guard(*this);

    auto ite = unused_sockets_.find(internal);
    if (ite != unused_sockets_.end()) {
      unused_sockets_.erase(ite);
    } else {
      new_connections_.erase(internal.get());
      unreferenced_sockets_.insert(internal);
    }
  }

  void InternalTimeWait(const std::shared_ptr<SocketInternal> &internal,
                        const SocketIdentifier &id) {
    timeout_queue_.PushEvent([internal, id, this]() {
          internal->Reset();
          InternalClosed(internal, id);
          return false;
        }, std::chrono::seconds(5));
  }

  void InternalClosed(const std::shared_ptr<SocketInternal> &internal,
                      const SocketIdentifier &id) {
    std::lock_guard guard(*this);
    identifier_to_socket_.erase(id);
    used_port_.erase(id);
    new_connections_.erase(internal.get());
  }

  uint16_t GetPortNumber(uint32_t peer_ip, uint16_t peer_port) {
    std::lock_guard guard(*this);
    static std::mt19937 e(std::random_device{}());
    static std::uniform_int_distribution<uint16_t> d(
        1, std::numeric_limits<uint16_t>::max());
    
    
    SocketIdentifier id(ip_, 0, peer_ip, peer_port);
    for (int i=0; i<65536; ++i) {
      const auto port = d(e);
      id.SetHostPort(port);
      if (used_port_.find(id) == used_port_.end())
        return port;
    }
      
    throw std::runtime_error("Failed in allocating port number");
  }

  void ReceivePacket(std::shared_ptr<TcpPacket> packet) {
    const auto host_ip = ip_;
    const auto host_port = packet->GetHeader().DestinationPort();

    const auto peer_ip = packet->GetHeader().SourceAddress();
    const auto peer_port = packet->GetHeader().SourcePort();

    auto [internal, found] =
        packet->GetHeader().Syn() && !packet->GetHeader().Ack() ?
          FindInternal(host_ip, host_port, 0, 0) :
          FindInternal(host_ip, host_port, peer_ip, peer_port);

    if (found) {
      std::cout << "Internal found" << std::endl;
      internal->RecvPacket(std::move(packet));
      
      std::lock_guard internal_guard(*internal);
      while (internal->IsAnyPacketForSending(internal_guard))
        SendPacket(internal->GetPacketForSending(internal_guard));
    } else {
      std::cout << "Sending Rst" << std::endl;
      auto rst_packet = MakeTcpPacket(0);
      RstHeader(packet->GetHeader(), &rst_packet->GetHeader());
      SendPacket(rst_packet);
    }
  }
  
  TcpSocket NewSocket() {
    std::lock_guard guard(*this);

    auto [ite_socket, created] = unused_sockets_.insert(
        std::make_shared<SocketInternal>(ip_, 0, this));
    assert(created);
  
    return *ite_socket;
  }

  void lock() {
    mtx_.lock();
  }

  bool try_lock() {
    return mtx_.try_lock();
  }

  void unlock() {
    mtx_.unlock();
  }

  std::vector<std::shared_ptr<TcpPacket>> GetPacketsForSending() {
    // lock TcpManager first
    std::vector<std::shared_ptr<TcpPacket>> packets;
    {
      for (auto &internal : sockets_wait_for_sending_) {
        std::lock_guard guard1(*internal);
        std::lock_guard guard2(*this);
        while (internal->IsAnyPacketForSending(guard1)) {
          auto packet = internal->GetPacketForSending(guard1);
          packets_.push_back(std::move(packet));
        }
      }
      sockets_wait_for_sending_.clear();
      packets.swap(packets_);
    }

    for (auto &packet : packets) {
      packet->GetHeader().Checksum() = 0;
      packet->GetHeader().Checksum() = CalculateChecksum(*packet);
    }

    return packets;
  }

private:
  void SendPacket(std::shared_ptr<TcpPacket> packet) {
    std::lock_guard guard(*this);
    std::cout << "New Packet" << std::endl;
    packets_.push_back(std::move(packet));
  }

  std::pair<std::shared_ptr<SocketInternal>, bool> FindInternal(
      uint32_t host_ip, uint16_t host_port, uint32_t peer_ip,
      uint16_t peer_port) {
    std::lock_guard guard(*this);
    const auto socket = identifier_to_socket_.find(SocketIdentifier{
        host_ip, host_port, peer_ip, peer_port});
    if (socket == identifier_to_socket_.end())
      return {{}, false};

    return {socket->second, true};
  }

  uint32_t ip_ = 0;

  std::unordered_set<SocketIdentifier> used_port_;

  std::unordered_set<std::shared_ptr<SocketInternal>> unused_sockets_;

  std::unordered_set<std::shared_ptr<SocketInternal>> unreferenced_sockets_;

  std::unordered_map<
      SocketIdentifier,
      std::shared_ptr<SocketInternal>> identifier_to_socket_;

  std::unordered_map<
      void *,
      std::list<std::shared_ptr<SocketInternal>>> new_connections_;
  
  std::unordered_set<std::shared_ptr<SocketInternal>> sockets_wait_for_sending_;

  std::vector<std::shared_ptr<TcpPacket>> packets_;

  TimeoutQueue timeout_queue_;

  std::mutex mtx_;
};

} // namespace tcp_stack

#endif // _TCP_STACK_SOCKET_MANAGER_H_
