#ifndef _TCP_STACK_TCP_SOCKET_H_
#define _TCP_STACK_TCP_SOCKET_H_

#include <arpa/inet.h>
#include <cmath>

#include <memory>

#include "safe-log.h"
#include "socket-internal.h"

namespace tcp_stack {
class TcpSocket {
public:
  friend class SocketManager;

  TcpSocket() = default;

  TcpSocket(const TcpSocket &) = delete;
  TcpSocket(TcpSocket &&) = default;

  TcpSocket &operator=(const TcpSocket &) = delete;
  TcpSocket &operator=(TcpSocket &&) = default;

  ~TcpSocket() try {
    Log(__func__);
    if (internal_) {
      internal_->SocketClose();
      internal_->SocketDestroyed();
    }
  } catch(...) {}

  void Listen(uint16_t port) {
    if (!internal_)
      throw std::runtime_error("Invalid Socket");
    internal_->SocketListen(port);
  }
  
  TcpSocket Accept() {
    if (!internal_)
      throw std::runtime_error("Invalid Socket");
    return internal_->SocketAccept();
  }

  void Connect(const char *ip, uint16_t port) {
    if (!internal_)
      throw std::runtime_error("Invalid Socket");
    const uint32_t int_ip = ntohl(inet_addr(ip));
    internal_->SocketConnect(int_ip, port);
  }

  void Send(const char *first, size_t size) {
    if (!internal_)
      throw std::runtime_error("Invalid Socket");
    internal_->SocketSend(first, size);
  }
  
  size_t Recv(char *first, size_t size) {
    if (!internal_)
      throw std::runtime_error("Invalid Socket");
    return internal_->SocketRecv(first, size);
  }

  void Close() {
    if (!internal_)
      throw std::runtime_error("Invalid Socket");
    internal_->SocketClose();
    internal_.reset();
  }

private:
  TcpSocket(std::shared_ptr<SocketInternal> internal) : internal_(internal) {}
  std::shared_ptr<SocketInternal> internal_;
};

} // namespace tcp_stack

#endif // _TCP_STACK_TCP_SOCKET_H_
