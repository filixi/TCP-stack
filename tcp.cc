#include "tcp.h"

TcpSocket TcpInternal::AcceptConnection() {
  return manager_->AcceptConnection(this);
}

int TcpInternal::CloseInternal() {
  return manager_->CloseInternal(id_);
}