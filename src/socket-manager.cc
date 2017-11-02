#include "socket-manager.h"

#include "network-service.h"

namespace tcp_stack {
void SocketManager::SendPacket(std::shared_ptr<TcpPacket> packet) {
  Log("New Packet");

  packet->GetHeader().Checksum() = 0;
  packet->GetHeader().Checksum() = CalculateChecksum(*packet);
  network_service_->SendPacket(std::move(packet));
}

} // namespace tcp_stack