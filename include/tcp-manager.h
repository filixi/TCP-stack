#ifndef _TCP_MANAGER_H_
#define _TCP_MANAGER_H_

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "tcp.h"
#include "tcp-buffer.h"
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

  TcpSocket NewSocket(uint16_t host_port) {
    auto result = tcp_internals_.emplace(
        next_tcp_id_, std::make_shared<TcpInternal>(
            next_tcp_id_, this, host_port, 0));
    ++next_tcp_id_;
    
    assert(result.second == true);
    return TcpSocket(result.first->second);
  }
  
  TcpSocket NewSocket(uint64_t id) {
    auto search_result = tcp_internals_.find(id);
    
    assert(search_result != tcp_internals_.end());
    return TcpSocket(search_result->second);
  }

  void RegestBound(uint16_t host_port, uint16_t peer_port,
                   uint64_t id) {
    port_to_id_[HashPort(host_port, peer_port)] = id;
  }
  
  TcpSocket AcceptConnection(TcpInternal *from) {
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

  void NewConnection(uint64_t id, TcpPackage package) {
    auto &header = package.GetHeader();
    auto internal = NewInternal(header.DestinationPort(), header.SourcePort());
    internal->ReceivePackage(std::move(package));
    
    if (internal->GetState() != State::kClosed)
      incomming_connections_[id].push_back(internal->Id());
  }

  void SendPackage(std::shared_ptr<NetworkPackage> package) {
    packages_for_sending_.emplace_back(package);
  }
  
  template <class Container>
  void SendPackages(const Container &container) {
    for (const auto &package : container)
      SendPackage(static_cast<std::shared_ptr<NetworkPackage>>(package));
  }

  auto GetInternal(uint16_t host_port, uint16_t peer_port) {
    auto id_ite = port_to_id_.find(HashPort(host_port, peer_port));
    uint64_t id = 0;
    if (id_ite != port_to_id_.end())
      id = id_ite->second;
    return tcp_internals_.find(id);
  }
  
  int CloseInternal(uint64_t id);
  
  void Multiplexing(std::shared_ptr<NetworkPackage> package);
  
  void Interrupt() {}
  
  void SwapPackagesForSending(
      std::list<std::shared_ptr<NetworkPackage> > &list);
  
  std::chrono::time_point<std::chrono::steady_clock> GetNextEventTime() {
    //if (!timeout_queue_.empty())
    //  return timeout_queue_.begin()->first;
    return std::chrono::time_point<std::chrono::steady_clock>(
        std::chrono::steady_clock::now()) + std::chrono::milliseconds(177);
  }
  
 private:
  static uint32_t HashPort(uint16_t host_port, uint16_t peer_port) {
    return (static_cast<uint32_t>(host_port)<<16) + peer_port;
  }
  
  std::shared_ptr<TcpInternal> NewInternal(
      uint16_t host_port,uint16_t peer_port) {
    auto result = tcp_internals_.emplace(
        next_tcp_id_, std::make_shared<TcpInternal>(
            next_tcp_id_, this, host_port, peer_port));
    RegestBound(host_port, peer_port, next_tcp_id_);
    
    ++next_tcp_id_;
    return result.first->second;
  }
  
  std::list<std::shared_ptr<NetworkPackage> > packages_for_sending_;
  
  std::map<uint64_t, std::shared_ptr<TcpInternal> > tcp_internals_;
  std::map<uint64_t, std::list<uint64_t> > incomming_connections_;
  std::map<uint32_t, uint64_t> port_to_id_;
  
  std::multimap<std::chrono::time_point<std::chrono::steady_clock>,
                std::weak_ptr<TcpInternal> > timeout_queue_;
  
  uint64_t next_tcp_id_ = 1;
};

} // namespace tcp_simulator

#endif // _TCP_MANAGER_H_
