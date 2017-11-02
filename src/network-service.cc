#include "network-service.h"

namespace tcp_stack {
void LittleUdpSender(uint16_t port) {
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
  int peer_port = port;

  sockaddr_in peer_addr{AF_INET, htons(peer_port)};
  if (inet_pton(AF_INET, peer_address.c_str(), &peer_addr.sin_addr) != 1) 
    throw std::runtime_error("inet_pton failed with : " + peer_address);

  sendto(local, "begin", 5, 0, 
         (sockaddr *)&peer_addr, sizeof(sockaddr_in));
}

void NetworkService::Run(std::promise<void> running) {
  Log(__func__);
  host_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (host_socket_ < 0)
    throw std::runtime_error("socket errer");
  if (bind(host_socket_, (sockaddr *)&host_addr_, sizeof(sockaddr_in)))
    throw std::runtime_error("bind error");
  running.set_value();
  
  std::unique_ptr<char[]> buff(new char[102400]);

  for(;;) {
    pollfd fdarray[1] = {{host_socket_, POLLIN, 0}};
    auto ret = poll(fdarray, 1, 1000);

    if (ret < 0) {
      ;
    } else if (ret == 1) {
      Log("Packet receiving");
      auto n = recvfrom(host_socket_, buff.get(), 102400, 0,
                        nullptr, nullptr);
      if (n <= 0) {
        Log("Recvfrom error");
      } else {
        Log("Packet received");
        auto packet = MakeNetPacket(buff.get(), n);
        socket_manager_.ReceivePacket(packet);
      }
    } else {
      ;
    }
  }
}

} // namespace tcp_simulator
