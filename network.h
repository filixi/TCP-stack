#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <arpa/inet.h>
#include <sys/socket.h>

#include <atomic>
#include <memory>
#include <thread>

#include "tcp.h"

class NetworkService {
 public:
  NetworkService() = default;
  NetworkService(const NetworkService &) = delete;
  NetworkService(NetworkService &&) = delete;
  
  ~NetworkService() {
    Terminate();
  }
  
  NetworkService &operator=(const NetworkService &) = delete;
  NetworkService &operator=(NetworkService &&) = delete;
  
  static std::shared_ptr<NetworkService> AsyncRun() {
    auto ptr = std::make_shared<NetworkService>();
    ptr->thread_ = std::thread([ptr]{ptr->Run();});
    return ptr;
  }
  
  TcpSocket NewTcpSocket() {
    return tcp_manager_.NewSocket();
  }
  
  void Run() {
    // pselect
  }
  
  void Terminate() {
    terminate_flag_.store(true);
    if (thread_.joinable())
      thread_.join();
  }
  
 private:
  std::thread thread_;
  std::atomic<bool> terminate_flag_{false};
  
  TcpManager tcp_manager_;
};

void LittleUdpSender() {
  std::string local_address = "127.0.0.1";
  int local_port = 9562;
  int local;
  
  sockaddr_in sock_addr{AF_INET, htons(local_port)};
  if (inet_pton(AF_INET, local_address.c_str(), &sock_addr.sin_addr) != 1)
    throw std::runtime_error("inet_pton failed with : " + local_address);
  
  if ((local = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    throw std::runtime_error("socket AF_INET SOCK_DGRAM failed");
  
  if (bind(local, (sockaddr *)&sock_addr, sizeof(sockaddr_in)))
    throw std::runtime_error("bind failed");
  
  
  std::string peer_address = "127.0.0.1";
  int peer_port = 9563;

  sockaddr_in peer_addr{AF_INET, htons(peer_port)};
  if (inet_pton(AF_INET, peer_address.c_str(), &peer_addr.sin_addr) != 1) 
    throw std::runtime_error("inet_pton failed with : " + peer_address);
  

  sendto(local, "begin", 5, 0, 
         (sockaddr *)&peer_addr, sizeof(sockaddr_in));
}

#endif // _NETWORK_H_