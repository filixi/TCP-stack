#include "network.h"

std::mutex g_mtx;

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