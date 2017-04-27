#ifndef _TCP_SOCKET_H_
#define _TCP_SOCKET_H_

#include <list>
#include <memory>
#include <utility>

#include "tcp-buffer.h"
#include "tcp-internal.h"

namespace tcp_simulator {

class TcpSocket {
 public:
  TcpSocket() {}
  TcpSocket(std::weak_ptr<TcpInternal> internal) : internal_(internal) {}
  
  int Listen(uint16_t port) {
    return internal_.lock()->SocketListen(port);
  }
  
  TcpSocket Accept();
  
  void Connect(uint16_t port);
  
  auto Read() {
    using PacketsType = std::list<TcpPacket>;
    if (internal_.expired())
      return std::make_pair(PacketsType{}, false);
    return std::make_pair(internal_.lock()->SocketGetReceivedPackets(), true);
  }
  
  int Write(const char *first, const char *last) {
    return internal_.lock()->SocketAddPacketForSending(first, last);;
  }
  
  int Close() {
    if (internal_.expired())
      return -1;
    internal_.lock()->SocketCloseConnection();
    return 0;
  }
  
 private:
  std::weak_ptr<TcpInternal> internal_;
};

} // namespace tcp_simulator

#endif // _TCP_SOCKET_H_
