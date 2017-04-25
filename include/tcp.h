#ifndef _TCP_H_
#define _TCP_H_

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "tcp-buffer.h"
#include "tcp-state-machine.h"

class TcpSocket;
class TcpManager;

class TcpInternal : public TcpInternalInterface {
 public:
  TcpInternal(uint64_t id, TcpManager *manager, uint16_t host_port,
              uint16_t peer_port)
      : id_(id), state_(this), manager_(manager), host_port_(host_port),
        peer_port_(peer_port) {}
  
  TcpInternal(const TcpInternal &) = delete;
  TcpInternal(TcpInternal &&) = delete;
  
  TcpInternal &operator=(const TcpInternal &) = delete;
  TcpInternal &operator=(TcpInternal &&) = delete;
  
  // Api for Socket
  
  int CloseInternal(); 
  TcpSocket AcceptConnection();
  
  auto Id() const {
    return id_;
  }
  
  int Listen(uint16_t port);
  void Connect(uint16_t port, uint32_t seq, uint16_t window);
  
  void CloseConnection() {
    auto result = state_.SendFin();
    assert(result);
    unsequenced_packages_.clear();
  }
  
  std::list<TcpPackage> GetReceivedPackages() {
    return buffer_.GetReadPackages();
  }
  
  int AddPackageForSending(TcpPackage package);
  int AddPackageForSending(const char *begin, const char *end);
  
  // Api for TcpManager/StateMachine
  
  std::list<std::shared_ptr<NetworkPackage> > GetPackagesForSending();
  std::list<std::shared_ptr<NetworkPackage> > GetPackagesForResending();
  
  void ReceivePackage(TcpPackage package);
  
  auto HostPort() {
    return host_port_;
  }
  
  auto PeerPort() {
    return peer_port_;
  }
  
  void Reset() override;
  
  auto GetRstPackage() {
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetRst(true);
    package.GetHeader().SetAck(true);

    return package;
  }
  
  State GetState() {
    return state_;
  }

 private:
  // callback action
  void SendAck() override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetAck(true);

    AddPackageForSending(std::move(package));
  }
  
  void SendConditionAck() override {
    std::cerr << __func__ << std::endl;
    if (state_.GetLastPackageSize() == 0)
      return ;
    
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetAck(true);

    AddPackageForSending(std::move(package));
  }
  
  void SendSyn() override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetSyn(true);

    AddPackageForSending(std::move(package));
  }
  void SendSynAck() override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetSyn(true);
    package.GetHeader().SetAck(true);

    AddPackageForSending(std::move(package));
  }
  
  void SendFin() override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetFin(true);
    package.GetHeader().SetAck(true);

    AddPackageForSending(std::move(package));
  }
  
  void SendRst() override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetRst(true);
    package.GetHeader().SetAck(true);

    AddPackageForSending(std::move(package));
  }
  
  void Accept() override {
    std::cerr << __func__ << std::endl;
    unsequenced_packages_.emplace(
        current_package_.GetHeader().SequenceNumber(),
        std::move(current_package_));
  }
  
  void Discard() override {
    std::cerr << __func__ << std::endl;
  }
  
  void Close() override {
    std::cerr << __func__ << std::endl;
    Reset();
  }
  
  void NewConnection() override;
  
  uint64_t id_;
  
  TcpBuffer buffer_;
  TcpStateMachine state_;
  
  TcpManager *manager_;
  
  TcpPackage current_package_;

  uint16_t host_port_;
  uint16_t peer_port_;
  
  std::map<uint32_t, TcpPackage> unsequenced_packages_;
  
  std::mutex mtx_;
};

class TcpSocket {
 public:
  TcpSocket() {}
  TcpSocket(std::weak_ptr<TcpInternal> internal) : internal_(internal) {}
  
  int Listen(uint16_t port) {
    return internal_.lock()->Listen(port);
  }
  
  TcpSocket Accept();
  
  void Connect(uint16_t port);
  
  auto Read() {
    using PackagesType = std::list<TcpPackage>;
    if (internal_.expired())
      return std::make_pair(false, PackagesType{});
    return std::make_pair(true, internal_.lock()->GetReceivedPackages());
  }
  
  // haven't
  int Write(const char *first, const char *last) {
    return internal_.lock()->AddPackageForSending(first, last);;
  }
  
  int Close() {
    if (internal_.expired())
      return -1;
    internal_.lock()->CloseConnection();
    return 0;
  }
  
 private:
  std::weak_ptr<TcpInternal> internal_;
};

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
  void SendPackages(Container container) {
    for (const auto &package : container)
      SendPackage(static_cast<std::shared_ptr<NetworkPackage>>(package));
  }
  
  static uint32_t HashPort(uint16_t host_port, uint16_t peer_port) {
    return (static_cast<uint32_t>(host_port)<<16) + peer_port;
  }
  
  auto GetInternal(uint16_t host_port, uint16_t peer_port) {
    auto id_ite = port_to_id_.find(HashPort(host_port, peer_port));
    uint64_t id = 0;
    if (id_ite != port_to_id_.end())
      id = id_ite->second;
    return tcp_internals_.find(id);
  }
  
  int CloseInternal(uint64_t id) {
    std::cerr << "erasing internal" << std::endl;
    int ret = 0;
    auto ite_internal = tcp_internals_.find(id);
    
    if(ite_internal == tcp_internals_.end())
      ret = -1;
    else
      tcp_internals_.erase(ite_internal);
    
    auto connections = incomming_connections_.find(id);
    if (connections == incomming_connections_.end()) {
      ret = -1;
    } else {
      for (auto connection_id : connections->second) {
        std::cerr << "Non accpeted connection removed" << std::endl;
        auto ite_connection = tcp_internals_.find(connection_id);
        assert(ite_connection != tcp_internals_.end());
        auto package = ite_connection->second->GetRstPackage();
        ite_connection->second->Reset();
        
        SendPackage(static_cast<std::shared_ptr<NetworkPackage>>(package));
      }
    }
    
    std::cerr << "internal erased" << std::endl;
    return ret;
  }
  
  void Multiplexing(std::shared_ptr<NetworkPackage> package) {
    // Construct A TcpPackage
    TcpPackage tcp_package(package);
    
    std::cerr << __func__ << std::endl;
    
    std::cerr << tcp_package << std::endl;

    // find the TcpInternal according to the TcpPackage header
    auto ite = GetInternal(tcp_package.GetHeader().DestinationPort(),
                           tcp_package.GetHeader().SourcePort());
    
    if (ite == tcp_internals_.end()) {
      std::cerr << "searching for listenning internal" << std::endl;
      ite = GetInternal(tcp_package.GetHeader().DestinationPort(), 0);
    }
    
    // internal not found
    if (ite == tcp_internals_.end()) {
      // send RST
      std::cerr << "internal not found!" << std::endl;
      SendPackage(static_cast<std::shared_ptr<NetworkPackage> >(
          HeaderFactory().RstHeader(tcp_package)));
      return ;
    }
    
    auto &internal = ite->second;

    if (internal->GetState() == State::kListen) {
      std::cerr << "internal is listenning" << std::endl;
      internal->ReceivePackage(std::move(tcp_package));
    } else {
      internal->ReceivePackage(std::move(tcp_package));
      std::cerr << "internal has received package" << std::endl;
      if (internal->GetState() == State::kClosed) {
        
        CloseInternal(internal->Id());
        std::cerr << "internal erased" << std::endl;
        return ;
      }
    }
    
    // Ask for sending
    SendPackages(internal->GetPackagesForSending());
    //SendPackages(internal->GetPackagesForResending());

    // Add to resend queue
    timeout_queue_.emplace(
        std::chrono::time_point<
            std::chrono::steady_clock>(std::chrono::seconds(5)), internal);
  }
  
  void Interrupt() {
    
  }
  
  void SwapPackagesForSending(
      std::list<std::shared_ptr<NetworkPackage> > &list) {
    for (auto pair : tcp_internals_) {
      auto &internal = pair.second;
      SendPackages(internal->GetPackagesForSending());
    }
    
    packages_for_sending_.swap(list);
  }
  
  void lock() {}
  void unlock() {
    std::cerr << "unlock" << std::endl;
  }
  
 private:
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

#endif // _TCP_H_
