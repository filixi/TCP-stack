#ifndef _TCP_STACK_NETWORK_H_
#define _TCP_STACK_NETWORK_H_

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include "socket-manager.h"
#include "safe-log.h"

namespace tcp_stack {
class NetworkService {
 public:
  NetworkService(const std::string &host_address, uint16_t host_port,
                 const std::string &peer_address, uint16_t peer_port)
      : host_addr_{AF_INET, htons(host_port)},
        host_port_(host_port),
        peer_addr_{AF_INET, htons(peer_port)},
        peer_port_(peer_port),
        socket_manager_(ntohl(inet_addr(host_address.c_str())), this) {
    if (inet_pton(AF_INET, host_address.c_str(), &host_addr_.sin_addr) != 1)
      throw std::runtime_error("inet_pton failed with: " + host_address);
    if (inet_pton(AF_INET, peer_address.c_str(), &peer_addr_.sin_addr) != 1)
      throw std::runtime_error("inet_pton failed with: " + peer_address);
  }
  
  NetworkService(const NetworkService &) = delete;
  NetworkService(NetworkService &&) = delete;
  
  ~NetworkService() {
    Terminate();
  }
  
  NetworkService &operator=(const NetworkService &) = delete;
  NetworkService &operator=(NetworkService &&) = delete;
  
  template <class... Args>
  static std::shared_ptr<NetworkService> AsyncRun(Args&&... args) {
    auto ptr = std::make_shared<NetworkService>(std::forward<Args>(args)...);
    std::promise<void> running;
    auto future = running.get_future();
    ptr->thread_ = std::thread([ptr, running(std::move(running))]() mutable {
          ptr->Run(std::move(running));
        });
    
    future.get();
    return ptr;
  }
  
  TcpSocket NewSocket() {
    return socket_manager_.NewSocket();
  }

  void SendPacket(std::shared_ptr<TcpPacket> packet) {
    auto [buff, size] = packet->GetBuffer();
    sendto(host_socket_, buff, size, 0,
        (sockaddr *)&peer_addr_, sizeof(sockaddr_in));
  }

  void Terminate() {
    terminate_flag_.store(true);
    if (thread_.joinable())
      thread_.join();
  }
  
  void join() {
    thread_.join();
  }

 private:
  void Run(std::promise<void> running);
  
  std::atomic<bool> terminate_flag_{false};
  std::thread thread_;

  sockaddr_in host_addr_;
  uint16_t host_port_;
  sockaddr_in peer_addr_;
  uint16_t peer_port_;

  int host_socket_;
  
  SocketManager socket_manager_;
};

void LittleUdpSender(uint16_t port);

} // namespace tcp_stack

#endif // _TCP_STACK_NETWORK_H_
