#ifndef _TCP_H_
#define _TCP_H_

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <utility>

#include "tcp-buffer.h"
#include "tcp-state-machine.h"

namespace tcp_simulator {

class TcpSocket;
class TcpManager;

class TcpInternal : public TcpInternalInterface {
 public:
  TcpInternal(uint64_t id, TcpManager *manager, uint16_t host_port,
              uint16_t peer_port)
      : id_(id), manager_(manager), host_port_(host_port),
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
    auto result = state_.FinSent();
    assert(result);
    SendFin();
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

} // namespace tcp_simulator

#endif // _TCP_H_
