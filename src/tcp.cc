#include "tcp.h"

#include "tcp-manager.h"

namespace tcp_simulator {

std::list<std::shared_ptr<NetworkPacket> >
TcpInternal::GetPacketsForSending() {
  std::list<std::shared_ptr<NetworkPacket> > result;
  for (;;) {
    auto packet = buffer_.GetFrontWritePacket();
    if (packet == nullptr)
      break;
    
    std::cerr << __func__ << " @ ";
    std::cerr << "GetSeq" << packet->Length() << " ";
    auto sequence_number = state_.NextSequenceNumber(
        static_cast<uint16_t>(packet->Length()));
    if (sequence_number.second == false)
      break;
    std::cerr << "Seq for sending:" << sequence_number.first << std::endl;
    packet->GetHeader().SequenceNumber() = sequence_number.first;
    
    std::cerr << "Psh ";
    std::cerr << std::endl;
    state_.PrepareHeader(packet->GetHeader(), packet->Length());
    packet->GetHeader().Checksum() = 0;
    packet->GetHeader().Checksum() = packet->CalculateChecksum();
    result.emplace_back(static_cast<std::shared_ptr<NetworkPacket> >(
        *packet));
    buffer_.MoveFrontWriteToUnack();
  }
  
  return result;
}

std::list<std::shared_ptr<NetworkPacket> >
TcpInternal::GetPacketsForResending() {
  return buffer_.GetPacketsForResending(
      [this](TcpPacket &packet) {
        state_.PrepareResendHeader(packet.GetHeader(), 0);
        packet.GetHeader().Checksum() = 0;
        packet.GetHeader().Checksum() = packet.CalculateChecksum();
      });
}

int TcpInternal::AddPacketForSending(TcpPacket packet) {
  packet.GetHeader().SourcePort() = host_port_;
  packet.GetHeader().DestinationPort() = peer_port_;
  buffer_.AddToWriteBuffer(std::move(packet));
  return 0;
}

void TcpInternal::ReceivePacket(TcpPacket packet) {
  if (packet.ValidByChecksum() == false) {
    std::cerr << "Checksum failed" << std::endl;
    Discard();
    return ;
  }
  current_packet_ = std::move(packet);
  auto size = current_packet_.Length();
  
  auto react = state_.OnReceivePacket(&current_packet_.GetHeader(), size);
  std::cerr << "Reacting" << std::endl;
  react(this);
  
  std::cerr << "Checking unsequenced packets" << std::endl;
  while (!unsequenced_packets_.empty()) {
    std::cerr << "Check" << std::endl;
    auto ite = unsequenced_packets_.begin();
    auto &packet = ite->second;
    if (!state_.OnSequencePacket(packet.GetHeader(), packet.Length()))
      break;
    std::cerr << "A packet is added to ReadBuffer" << std::endl;
    buffer_.AddToReadBuffer(std::move(packet));
    unsequenced_packets_.erase(ite);
  }
  
  std::cerr << "#" << id_ << state_;
}

TcpSocket TcpInternal::SocketAcceptConnection() {
  std::lock_guard<TcpManager> guard(tcp_manager_);
  return tcp_manager_.AcceptConnection(this, guard);
}

void TcpInternal::Reset() {
  std::cerr << __func__ << std::endl;
  state_.SetState(0, State::kClosed, Stage::kClosed);
  unsequenced_packets_.clear();
  buffer_.Clear();
  host_port_ = peer_port_ = 0;
}

void TcpInternal::NewConnection() {
  std::cerr << __func__ << std::endl;
  tcp_manager_.NewConnection(id_, std::move(current_packet_));
}

void TcpInternal::Connect(uint16_t port, uint32_t seq, uint16_t window,
    const std::lock_guard<TcpManager> &guard) {
  peer_port_ = port;
  assert(host_port_ != 0 && peer_port_ != 0);
  
  tcp_manager_.Bind(host_port_, peer_port_, id_, guard);
  auto result = state_.SynSent(seq, window);
  assert(result);
  SendSyn();
}

int TcpInternal::Listen(uint16_t port,
    const std::lock_guard<TcpManager> &guard) {
  assert(host_port_ != 0);
  auto result = state_.Listen();
  assert(result);
  
  host_port_ = port;
  peer_port_ = 0;
  tcp_manager_.Bind(host_port_, peer_port_, id_, guard);
  return 0;
}

TcpSocket TcpSocket::Accept() {
  return internal_.lock()->SocketAcceptConnection();
}

void TcpSocket::Connect(uint16_t port) {
  internal_.lock()->SocketConnect(port, 1425, 10240);
}

} // namespace tcp_simulator
