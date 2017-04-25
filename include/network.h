#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "tcp.h"

extern std::mutex g_mtx;

class NetworkService {
 public:
  NetworkService(const std::string &host_address, uint16_t host_port,
                 const std::string &peer_address, uint16_t peer_port)
      : host_addr_{AF_INET, htons(host_port)},
        host_port_(host_port),
        peer_addr_{AF_INET, htons(peer_port)},
        peer_port_(peer_port) {
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
    ptr->thread_ = std::thread([ptr]{ptr->Run();});
    return ptr;
  }
  
  TcpSocket NewTcpSocket(uint16_t host_port) {
    return tcp_manager_.NewSocket(host_port);
  }
  
  void Run() {
    std::cerr << __func__ << std::endl;
    std::list<std::shared_ptr<NetworkPackage> > packages_for_sending;
    int host_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (host_socket < 0)
      throw std::runtime_error("socket errer");
    bind(host_socket, (sockaddr *)&host_addr_, sizeof(sockaddr_in));
    std::unique_ptr<char[]> buff(new char[102400]);
    
    for (int i = 0; i<30; ++i) {
      pollfd fdarray[1] = {{host_socket, POLLIN, 0}};
      auto ret = poll(fdarray, 1, 177);
      
      std::lock_guard<std::mutex> guard(g_mtx);
      std::cerr << this << std::endl;
      
      if (ret < 0) {
        if (ret != EINTR) {
          std::cerr << "1";
        }
          
      } else if (ret == 1) {
        std::cerr << "Package receiving" << std::endl;
        auto n = recvfrom(host_socket, buff.get(), 102400, 0,
                          nullptr, nullptr);
        if (n <= 0) {
          std::cerr << "2";
        } else {
          std::cerr << "Package received" << std::endl;
          auto package = NetworkPackage::NewPackage(buff.get(), buff.get()+n);
          tcp_manager_.Multiplexing(package);
        }
      } else {
        ;
      }
      
      tcp_manager_.SwapPackagesForSending(packages_for_sending);
      for (auto &package : packages_for_sending) {
        char *begin = nullptr, *end = nullptr;
        std::tie(begin, end) = package->GetBuffer();
        sendto(host_socket, begin, static_cast<size_t>(end-begin), 0,
               (sockaddr *)&peer_addr_, sizeof(sockaddr_in));
        std::cerr << "Send package" << std::endl;
      }
      packages_for_sending.clear();
      
      std::cerr << std::endl;
    }
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
  std::thread thread_;
  std::atomic<bool> terminate_flag_{false};
  
  sockaddr_in host_addr_;
  uint16_t host_port_;
  sockaddr_in peer_addr_;
  uint16_t peer_port_;
  
  TcpManager tcp_manager_;
};

void LittleUdpSender(uint16_t port);

#endif // _NETWORK_H_