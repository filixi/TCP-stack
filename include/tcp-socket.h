#ifndef _TCP_STACK_TCP_SOCKET_H_
#define _TCP_STACK_TCP_SOCKET_H_

#include <cmath>

#include <memory>

#include "safe-log.h"
#include "socket-internal.h"

namespace tcp_stack {
inline int IpStr2Int(const char *ip, uint64_t *result) {
  auto &r = *result;
  const char *p = ip;

  r = atoi(p);
  for (int i=0; i<3; ++i) {
    while (ip-p<=3 && *p && *p!='.')
      ++p;
    if (ip-p > 3 || !*p)
      return -1;

    ++p;
    r = (r<<8) | atoi(p);
    ip = p;
  }

  return 0;
}

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
    uint64_t int_ip = 0;
    IpStr2Int(ip, &int_ip);
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
