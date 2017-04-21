#ifndef _TCP_H_
#define _TCP_H_

#include <iostream>
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
  friend struct DebugTcp;
  
  TcpInternal(uint64_t id, TcpManager *manager, uint16_t host_port,
              uint16_t peer_port)
      : id_(id), state_(this), manager_(manager), host_port_(host_port),
        peer_port_(peer_port) {}
  
  TcpInternal(const TcpInternal &) = delete;
  TcpInternal(TcpInternal &&) = default;
  
  TcpInternal &operator=(const TcpInternal &) = delete;
  TcpInternal &operator=(TcpInternal &&) = delete;
  
  int CloseInternal();  
  TcpSocket AcceptConnection();
  
  std::list<TcpPackage> GetReceivedPackages() {
    return buffer_.GetReadPackages();
  }
  
  std::list<std::shared_ptr<NetworkPackage> > GetPackagesForSending() {
    std::list<std::shared_ptr<NetworkPackage> > result;
    for (;;) {
      auto package = buffer_.GetFrontWritePackage();
      if (package == nullptr)
        break;
      
      auto sequence_number = state_.GetSequenceNumber(package->Length());
      if (sequence_number.second == false)
        break;
      package->GetHeader().SequenceNumber() = sequence_number.first;
      
      state_.PrepareDataHeader(package->GetHeader(), package->Length());
      package->GetHeader().Checksum() = 0;
      package->GetHeader().Checksum() = package->CalculateChecksum();
      result.emplace_back(static_cast<std::shared_ptr<NetworkPackage> >(
          *package));
      buffer_.MoveFrontWriteToUnack();
    }
    
    return result;
  }
  
  std::list<std::shared_ptr<NetworkPackage> > GetPackagesForResending() {
    return buffer_.GetPackagesForResending(
        [this](TcpPackage &package) {
          state_.PrepareResendHeader(package.GetHeader());
          package.GetHeader().Checksum() = 0;
          package.GetHeader().Checksum() = package.CalculateChecksum();
        });
  }
  
  int AddPackageForSending(TcpPackage package) {
    package.GetHeader().SourcePort() = host_port_;
    package.GetHeader().DestinationPort() = peer_port_;
    buffer_.AddToWriteBuffer(std::move(package));
    return 0;
  }
  
  void ReceivePackage(TcpPackage package) {
    assert(state_ != Stage::kClosed);
    current_package_ = std::move(package);
    auto size = current_package_.Length();
    
    state_.OnReceivePackage(&current_package_.GetHeader(), size);
    while (!unsequenced_packages_.empty()) {
      auto ite = unsequenced_packages_.begin();
      auto &package = ite->second;
      if (!state_.OnSequencePackage(package.GetHeader(), package.Length()))
        break;
      buffer_.AddToReadBuffer(std::move(package));
      unsequenced_packages_.erase(ite);
    }
  }
  
  auto Id() const {
    return id_;
  }
  
  void Listen() {
    auto result = state_.Listen();
    assert(result);
  }
  
  void Connect(uint32_t seq, uint16_t window) {
    state_.SendSyn(seq, window);
  }
  
  void CloseConnection() {
    auto result = state_.SendFin();
    assert(result);
  }
  
 private:
  void SendAck() override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetAck(true);
    state_.PrepareSpecialHeader(package.GetHeader());
    
    AddPackageForSending(std::move(package));
  }
  
  void SendConditionAck() override {
    std::cerr << __func__ << std::endl;
    if (state_.GetLastPackageSize() == 0)
      return ;
    
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetAck(true);
    state_.PrepareSpecialHeader(package.GetHeader());
    
    AddPackageForSending(std::move(package));
  }
  
  void SendSyn(uint32_t, uint16_t) override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetSyn(true);
    package.GetHeader().SequenceNumber() = state_.GetInitialSequenceNumber();
    state_.PrepareSpecialHeader(package.GetHeader());
    
    AddPackageForSending(std::move(package));
  }
  void SendSynAck(uint32_t, uint16_t) override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetSyn(true);
    package.GetHeader().SetAck(true);
    package.GetHeader().SequenceNumber() = state_.GetInitialSequenceNumber();
    state_.PrepareSpecialHeader(package.GetHeader());
    
    AddPackageForSending(std::move(package));
  }
  
  void SendFin() override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetFin(true);
    package.GetHeader().SetAck(true);
    state_.PrepareSpecialHeader(package.GetHeader());
    
    AddPackageForSending(std::move(package));
  }
  
  void SendRst() override {
    std::cerr << __func__ << std::endl;
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetRst(true);
    package.GetHeader().SetAck(true);
    state_.PrepareSpecialHeader(package.GetHeader());
    
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
  }
  
  void NewConnection() override {
    std::cerr << __func__ << std::endl;
  }
  
  uint64_t id_;
  
  TcpBuffer buffer_;
  TcpStateMachine state_;
  
  TcpManager *manager_;
  
  TcpPackage current_package_;

  uint16_t host_port_;
  uint16_t peer_port_;
  
  std::map<uint32_t, TcpPackage> unsequenced_packages_;
  
  std::vector<std::function<int(int)> > actions_;
};

class TcpSocket {
 public:
  TcpSocket() {}
  TcpSocket(std::weak_ptr<TcpInternal> internal) : internal_(internal) {}
  
  int Listen() {
    return 0;
  }
  
  std::pair<bool, TcpSocket> Accept() {
    return {false, {}};
  }
  
  TcpSocket Connect() {
    // interrupt pselect
    
    return {};
  }
  
  auto Read() {
    using PackagesType = std::list<TcpPackage>;
    if (internal_.expired())
      return std::make_pair(false, PackagesType{});
    return std::make_pair(true, internal_.lock()->GetReceivedPackages());
  }
  
  // haven't
  int Write(const char *first, const char *last) {
    return 0;
  }
  
  int Close() {
    if (internal_.expired())
      return -1;
    
    auto ret = internal_.lock()->CloseInternal();
    internal_.reset();
    return ret;
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
  
  TcpSocket NewSocket() {
    std::unique_lock<std::mutex> lock(mtx_);
    auto result = tcp_internals_.emplace(
        next_tcp_id_, std::make_shared<TcpInternal>(next_tcp_id_, this, 0, 0));
    ++next_tcp_id_;
    lock.unlock();
    
    assert(result.second == true);
    return TcpSocket(result.first->second);
  }
  
  TcpSocket NewSocket(uint64_t id) {
    std::unique_lock<std::mutex> lock(mtx_);
    auto search_result = tcp_internals_.find(id);
    lock.unlock();
    
    assert(search_result != tcp_internals_.end());
    return TcpSocket(search_result->second);
  }
  
  TcpSocket AcceptConnection(TcpInternal *from) {
    uint64_t id = from->Id();
    auto search_result = incomming_connections_.find(id);
    assert(search_result != incomming_connections_.end());
    
    if (!search_result->second.empty()) {
      auto new_id = search_result->second.front();
      search_result->second.pop_front();
      return NewSocket(new_id);
    }
    return {};
  }
  
  int CloseInternal(uint64_t id) {
    assert(tcp_internals_.find(id) != tcp_internals_.end());
    // if listen
      // realease all waiting connection
    return 0;
  }
  
  void DeMultiplexing(std::shared_ptr<NetworkPackage> package) {
    // Construct A TcpPackage
    TcpPackage tcp_package(package);
    
    // find the TcpInternal according to the TcpPackage header
    
    // move it to the internal
    
    // if result == NewConnection
      // Insert a new connection to both tcp_internals
    
    // else if result == ConnectionClosed
      // Remove connection
    
    // Ask for sending
    
    // Add to resend queue
  }
  
 private:
  std::map<uint64_t, std::shared_ptr<TcpInternal> > tcp_internals_;
  std::map<uint64_t, std::list<uint64_t> > incomming_connections_;
  
  std::map<std::chrono::time_point<std::chrono::steady_clock>,
           std::weak_ptr<TcpInternal> > resend_queue_;
  
  uint64_t next_tcp_id_ = 1;

  std::mutex mtx_;
};

#endif // _TCP_H_