
#include "tcp-manager.h"

namespace tcp_simulator {

void TcpManager::Multiplexing(std::shared_ptr<NetworkPackage> package) {
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

int TcpManager::CloseInternal(uint64_t id) {
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

void TcpManager::SwapPackagesForSending(
    std::list<std::shared_ptr<NetworkPackage> > &list) {
  for (auto pair : tcp_internals_) {
    auto &internal = pair.second;
    SendPackages(internal->GetPackagesForSending());
  }
  
  packages_for_sending_.swap(list);
}

} // namespace tcp_simulator
