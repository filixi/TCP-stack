
#include "tcp-manager.h"

namespace tcp_simulator {

void ResendPacket::OnEvent() {
  // Add to resend
  auto packet = packet_.lock();
  auto internal = internal_.lock();
  if (packet != nullptr && internal != nullptr) {
    std::cout << internal->Id() << "Resending: " << TcpPacket(packet)
              << std::endl;
    
    internal_.lock()->PreparePacketForResending(packet);
    manager_.AddToResendList(std::move(packet));
  }
}

void CloseInternal::OnEvent() {
  manager_.CloseInternal(id_);
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
      tcp_internals_.erase(ite_connection);
      
      SendPacket(static_cast<std::shared_ptr<NetworkPacket>>(packet));
    }
  }
  
  std::cerr << id << "internal erased" << std::endl;
  return ret;
}

void TcpManager::Multiplexing(std::shared_ptr<NetworkPacket> packet,
    const std::lock_guard<TcpManager> &) {
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
      return ;
    } else if (internal->GetState() == State::kTimeWait) {
      AddEvent(std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(20000),
               std::make_shared<tcp_simulator::CloseInternal>(
                   *this, internal->Id()));
    }
  }
}

void TcpManager::SwapPacketsForSending(
    std::list<std::shared_ptr<NetworkPacket> > &list,
    const std::lock_guard<TcpManager> &) {
  for (auto pair : tcp_internals_) {
    auto &internal = pair.second;
    auto packets = internal->GetPacketsForSending();
    
    for (auto &packet : packets) {
      AddEvent(std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(2000),
               std::make_shared<ResendPacket>(
                   packet, *this, std::chrono::milliseconds(2000),internal));
    }
    SendPackets(std::move(packets));
  }
  
  using ClockType = decltype(timeout_queue_)::key_type::clock;
  std::list<std::shared_ptr<TimeoutEvent> > events_triggered;
  
  for (auto ite = timeout_queue_.begin(); ite != timeout_queue_.end();) {
    const std::chrono::seconds duration =
        std::chrono::duration_cast<std::chrono::seconds>(
            ite->first - ClockType::now());
    if (duration >= std::chrono::seconds(1))
      break;
    std::cerr << "Resend triggered" << std::endl;
    events_triggered.emplace_back(std::move(ite->second));
    ite = timeout_queue_.erase(ite);
  }
  
  for (auto &event : events_triggered) {
    event->OnEvent();
    auto deadline = event->Reschedule();
    if (deadline.second == true)
      AddEvent(deadline.first, std::move(event));
  }
  
  packets_for_sending_.swap(list);
}

} // namespace tcp_simulator
