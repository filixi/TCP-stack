#include "socket-internal.h"

#include "socket-manager.h"

namespace tcp_stack {
void SocketInternal::SendSyn(uint32_t seq, uint16_t window) {
  auto packet = MakeTcpPacket(0);
  SynHeader(seq, window, &packet->GetHeader());

  send_buffer_.InitializeAckNumber(seq + 1);

  peer_ip_ = next_peer_ip_;
  peer_port_ = next_peer_port_;

  host_port_ = manager_->GetPortNumber(peer_ip_, peer_port_);

  manager_->InternalConnectTo(
      shared_from_this(), host_port_, peer_ip_, peer_port_);

  SendPacketWithResend(std::move(packet));
}

std::shared_ptr<SocketInternal> SocketInternal::SocketAccept() {
  if (state_.GetState() != State::kListen)
    throw std::runtime_error("Socket is not listening");
  
  auto lock = manager_->SelfUniqueLock();
  wait_until_readable_.wait(lock, [this]() {
        return manager_->InternalAnyNewConnection(this);
      });

  return manager_->InternalGetNewConnection(this, GetIdentifier());
}

void SocketInternal::SendPacket(std::shared_ptr<TcpPacket> packet) {
  SetSource(host_ip_, host_port_, &packet->GetHeader());
  SetDestination(peer_ip_, peer_port_, &packet->GetHeader());

  manager_->InternalSendPacket(std::move(packet));
}

void SocketInternal::SendPacketWithResend(std::shared_ptr<TcpPacket> packet) {
  SetSource(host_ip_, host_port_, &packet->GetHeader());
  SetDestination(peer_ip_, peer_port_, &packet->GetHeader());

  auto predicate = [internal = this->weak_from_this()](auto &packet) {
        auto shared_self = internal.lock();
        if (!shared_self)
          return false;
        
        std::lock_guard guard(*shared_self);
        packet->GetHeader().AcknowledgementNumber() =
            shared_self->state_.GetControlBlock().rcv_nxt;
        if (shared_self->state_.GetState() == State::kClosed) {
          return false;
        } else if (packet->GetHeader().Syn() || packet->GetHeader().Fin()) {
          return shared_self->state_.GetControlBlock().snd_una <
                   packet->GetHeader().SequenceNumber() + 1;
        } else {
          return shared_self->state_.GetControlBlock().snd_una <
                   packet->GetHeader().SequenceNumber();
        }
      };
  manager_->InternalSendPacketWithResend(std::move(packet), predicate);
}

void SocketInternal::Listen() {
  host_port_ = next_host_port_;
  manager_->InternalListen(shared_from_this(), GetIdentifier());
}

void SocketInternal::NewConnection() {
  Log("New Connection");
  manager_->InternalNewConnection(this, current_packet_.lock());
}

void SocketInternal::SocketDestroyed() {
  manager_->InternalClosing(shared_from_this());
}

void SocketInternal::Close() {
  manager_->InternalClosed(shared_from_this(), GetIdentifier());
}

void SocketInternal::TimeWait() {
  manager_->InternalTimeWait(shared_from_this(), GetIdentifier());
}

void SocketInternal::SocketSend(const char *first, size_t size) {
  send_buffer_.Push(first,size);

  manager_->InternalHasPacketForSending(shared_from_this());
}

} // namespace tcp_stack
