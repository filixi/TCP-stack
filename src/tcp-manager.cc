
#include "tcp-manager.h"

namespace tcp_simulator {

void TcpManager::Multiplexing(std::shared_ptr<NetworkPacket> packet) {
  // Construct A TcpPacket
  TcpPacket tcp_packet(packet);
  
  std::cerr << __func__ << std::endl;
  std::cerr << tcp_packet << std::endl;

  // find the TcpInternal according to the TcpPacket header
  auto ite = GetInternal(tcp_packet.GetHeader().DestinationPort(),
                         tcp_packet.GetHeader().SourcePort());
  
  if (ite == tcp_internals_.end()) {
    std::cerr << "searching for listenning internal" << std::endl;
    ite = GetInternal(tcp_packet.GetHeader().DestinationPort(), 0);
  }
  
  // internal not foundf
  if (ite == tcp_internals_.end()) {
    // send RST
    std::cerr << "internal not found!" << std::endl;
    SendPacket(static_cast<std::shared_ptr<NetworkPacket> >(
        HeaderFactory().RstHeader(tcp_packet)));
    return ;
  }
  
  auto &internal = ite->second;

  if (internal->GetState() == State::kListen) {
    std::cerr << "internal is listenning" << std::endl;
    internal->ReceivePacket(std::move(tcp_packet));
  } else {
    internal->ReceivePacket(std::move(tcp_packet));
    std::cerr << "internal has received packet" << std::endl;
    if (internal->GetState() == State::kClosed) {
      
      CloseInternal(internal->Id());
      std::cerr << "internal erased" << std::endl;
      return ;
    }
  }
  
  // Ask for sending
  SendPackets(internal->GetPacketsForSending());
  //SendPackets(internal->GetPacketsForResending());

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
      auto packet = ite_connection->second->GetRstPacket();
      ite_connection->second->Reset();
      
      SendPacket(static_cast<std::shared_ptr<NetworkPacket>>(packet));
    }
  }
  
  std::cerr << "internal erased" << std::endl;
  return ret;
}

void TcpManager::SwapPacketsForSending(
    std::list<std::shared_ptr<NetworkPacket> > &list) {
  for (auto pair : tcp_internals_) {
    auto &internal = pair.second;
    SendPackets(internal->GetPacketsForSending());
  }
  
  packets_for_sending_.swap(list);
}

} // namespace tcp_simulator
