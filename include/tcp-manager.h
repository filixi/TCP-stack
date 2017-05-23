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

class TimeoutEvent {
 public:
  virtual void OnEvent() = 0;
  virtual std::pair<std::chrono::time_point<std::chrono::steady_clock>, bool>
  Reschedule() = 0;
};

class ResendPacket : public TimeoutEvent {
 public:
  ResendPacket(std::weak_ptr<NetworkPacket> packet, TcpManager &manager,
               std::chrono::milliseconds period,
               std::weak_ptr<TcpInternal> internal)
      : packet_(packet), manager_(manager), internal_(internal),
        period_(period) {}
      
  void OnEvent() override;

  std::pair<std::chrono::time_point<std::chrono::steady_clock>, bool>
  Reschedule() override {
    if (packet_.expired())
      return {std::chrono::steady_clock::now(), false};
    return {std::chrono::steady_clock::now() + period_, true};
  }

 private:
  std::weak_ptr<NetworkPacket> packet_;
  TcpManager &manager_;
  std::weak_ptr<TcpInternal> internal_;
  
  const std::chrono::milliseconds period_;
};


class CloseInternal : public TimeoutEvent {
 public:
  CloseInternal(TcpManager &manager, uint64_t id)
      : manager_(manager), id_(id) {}
        
  void OnEvent() override;
  
  std::pair<std::chrono::time_point<std::chrono::steady_clock>, bool>
  Reschedule() override {
    return {std::chrono::steady_clock::now(), false};
  }
  
  TcpManager &manager_;
  uint64_t id_;
};

class TcpManager {
 public:
  friend class ResendPacket;
  friend class CloseInternal;
  
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
    
    if (internal->GetState() != State::kClosed) {
      incomming_connections_[id].push_back(internal->Id());
      tcp_internals_[id]->NotifyOnNewConnection(
          incomming_connections_[id].size());
    }
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
    if (!timeout_queue_.empty())
      return timeout_queue_.begin()->first;
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
  
  void AddToResendList(std::shared_ptr<NetworkPacket> &&packet) {
    packets_for_sending_.emplace_back(std::move(packet));
  }
  
  void AddEvent(std::chrono::time_point<std::chrono::steady_clock> abs_time,
                std::shared_ptr<TimeoutEvent> &&event) {
    std::cerr << "Timeout event added: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     abs_time-std::chrono::steady_clock::now()).count()
              << std::endl;
    timeout_queue_.emplace(abs_time, std::move(event));
  }
  
  std::list<std::shared_ptr<NetworkPacket> > packets_for_sending_;
  
  std::map<uint64_t, std::shared_ptr<TcpInternal> > tcp_internals_;
  std::map<uint64_t, std::list<uint64_t> > incomming_connections_;
  std::map<uint32_t, uint64_t> port_to_id_;
  
  std::multimap<std::chrono::time_point<std::chrono::steady_clock>,
                std::shared_ptr<TimeoutEvent> > timeout_queue_;

  uint64_t next_tcp_id_ = 1;
  
  std::mutex mtx_;
};

} // namespace tcp_simulator

#endif // _TCP_MANAGER_H_
