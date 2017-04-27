#include "tcp-socket.h"

#include "tcp-manager.h"

namespace tcp_simulator {
  
TcpSocket TcpSocket::Accept() {
  return internal_.lock()->SocketAcceptConnection();
}

void TcpSocket::Connect(uint16_t port) {
  internal_.lock()->SocketConnect(port, 1425, 10240);
}

} // namespace tcp_simulator
