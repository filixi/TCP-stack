#ifndef _TCP_MANAGER_H_
#define _TCP_MANAGER_H_

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "tcp-buffer.h"
#include "tcp-internal.h"
#include "tcp-socket.h"
#include "tcp-state-machine.h"

namespace tcp_simulator {

class TcpManager {
 public:
  TcpManager() = default;
  TcpManager(const TcpManager &) = delete;
  TcpManager(TcpManager &&) = delete;
  
  TcpManager &operator=(const TcpManager &) = delete;
  TcpManager &operator=(TcpManager &&) = delete;

  ~TcpManager() = default;
  
  // API for user
  TcpSocket NewSocket(uint16_t host_port) {
    std::lock_guard<TcpManager> guard(*this);
    
    auto result = tcp_internals_.emplace(
        next_tcp_id_, std::make_shared<TcpInternal>(
            next_tcp_id_, *this, host_port, 0));
    ++next_tcp_id_;
    
    assert(result.second == true);
    return TcpSocket(result.first->second);
  }

  // API for Internal
  void Bind(uint16_t host_port, uint16_t peer_port, uint64_t id,
      const std::lock_guard<TcpManager> &) {
    Bind(host_port, peer_port, id);
  }
  
  TcpSocket AcceptConnection(TcpInternal *from,
      const std::lock_guard<TcpManager> &) {
    uint64_t id = from->Id();
    auto search_result = incomming_connections_.find(id);
    assert(search_result != incomming_connections_.end());
    
    if (!search_result->second.empty()) {
      auto new_id = search_result->second.front();
      search_result->second.pop_front();
      std::cerr << "connection poped" << std::endl;
      return NewSocket(new_id);
    }
    return {};
  }

  void NewConnection(uint64_t id, TcpPacket packet) {
    auto &header = packet.GetHeader();
    auto internal = NewInternal(header.DestinationPort(), header.SourcePort());
    internal->ReceivePacket(std::move(packet));
    
    if (internal->GetState() != State::kClosed)
      incomming_connections_[id].push_back(internal->Id());
  }

  int CloseInternal(uint64_t id, const std::lock_guard<TcpManager> &) {
    return CloseInternal(id);
  }

  // API for network Service
  void SwapPacketsForSending(
      std::list<std::shared_ptr<NetworkPacket> > &list,
      const std::lock_guard<TcpManager> &);
  
  void Multiplexing(std::shared_ptr<NetworkPacket> packet,
      const std::lock_guard<TcpManager> &);
  
  std::chrono::time_point<std::chrono::steady_clock> GetNextEventTime(
      const std::lock_guard<TcpManager> &) {
    //if (!timeout_queue_.empty())
    //  return timeout_queue_.begin()->first;
    return std::chrono::time_point<std::chrono::steady_clock>(
        std::chrono::steady_clock::now()) + std::chrono::milliseconds(177);
  }
  
  void lock() {
    mtx_.lock();
  }
  
  void unlock() {
    mtx_.unlock();
  }

 private:
  void Bind(uint16_t host_port, uint16_t peer_port, uint64_t id) {
    port_to_id_[HashPort(host_port, peer_port)] = id;
  }
  
  int CloseInternal(uint64_t id);
  
  void SendPacket(std::shared_ptr<NetworkPacket> packet) {
    packets_for_sending_.emplace_back(packet);
  }
  
  template <class Container>
  void SendPackets(const Container &container) {
    for (const auto &packet : container)
      SendPacket(static_cast<std::shared_ptr<NetworkPacket>>(packet));
  }
  
  void Interrupt() {}
  
  auto GetInternal(uint16_t host_port, uint16_t peer_port) {
    auto id_ite = port_to_id_.find(HashPort(host_port, peer_port));
    uint64_t id = 0;
    if (id_ite != port_to_id_.end())
      id = id_ite->second;
    return tcp_internals_.find(id);
  }
  
  TcpSocket NewSocket(uint64_t id) {
    auto search_result = tcp_internals_.find(id);
    
    assert(search_result != tcp_internals_.end());
    return TcpSocket(search_result->second);
  }
  
  static uint32_t HashPort(uint16_t host_port, uint16_t peer_port) {
    return (static_cast<uint32_t>(host_port)<<16) + peer_port;
  }
  
  std::shared_ptr<TcpInternal> NewInternal(
      uint16_t host_port,uint16_t peer_port) {
    auto result = tcp_internals_.emplace(
        next_tcp_id_, std::make_shared<TcpInternal>(
            next_tcp_id_, *this, host_port, peer_port));
    Bind(host_port, peer_port, next_tcp_id_);
    
    ++next_tcp_id_;
    return result.first->second;
  }
  
  std::list<std::shared_ptr<NetworkPacket> > packets_for_sending_;
  
  std::map<uint64_t, std::shared_ptr<TcpInternal> > tcp_internals_;
  std::map<uint64_t, std::list<uint64_t> > incomming_connections_;
  std::map<uint32_t, uint64_t> port_to_id_;
  
  std::multimap<std::chrono::time_point<std::chrono::steady_clock>,
                std::weak_ptr<TcpInternal> > timeout_queue_;
  
  uint64_t next_tcp_id_ = 1;
  
  std::mutex mtx_;
};

} // namespace tcp_simulator

#endif // _TCP_MANAGER_H_
