#include "network.h"

namespace tcp_simulator {

// lock for sequencing cerr
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

void NetworkService::Run(std::promise<void> running) {
  std::cerr << __func__ << std::endl;
  std::list<std::shared_ptr<NetworkPacket> > packets_for_sending;
  int host_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (host_socket < 0)
    throw std::runtime_error("socket errer");
  if (bind(host_socket, (sockaddr *)&host_addr_, sizeof(sockaddr_in)))
    throw std::runtime_error("bind error");
  running.set_value();
  
  std::unique_ptr<char[]> buff(new char[102400]);

  for(;;) {
    std::unique_lock<std::mutex> output_sequence_lock(g_mtx);
    
    pollfd fdarray[1] = {{host_socket, POLLIN, 0}};
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            tcp_manager_.GetNextEventTime(
                std::lock_guard<TcpManager>(tcp_manager_)) -
            std::chrono::steady_clock::now());
    //if (duration < std::chrono::milliseconds(10))
    duration = std::chrono::milliseconds(120);
    std::cerr << this << std::endl;
    std::cerr << "poll next time out : " << duration.count() << std::endl;
    
    output_sequence_lock.unlock();
    auto ret = poll(fdarray, 1, duration.count());
    output_sequence_lock.lock();
    
    
    if (ret < 0) {
      if (ret != EINTR) {
        std::cerr << "Interrupted";
      }
        
    } else if (ret == 1) {
      std::cerr << "Packet receiving" << std::endl;
      auto n = recvfrom(host_socket, buff.get(), 102400, 0,
                        nullptr, nullptr);
      if (n <= 0) {
        std::cerr << "Recvfrom error";
      } else {
        std::cerr << "Packet received" << std::endl;
        auto packet = NetworkPacket::NewPacket(buff.get(), buff.get()+n);
        tcp_manager_.Multiplexing(packet,
            std::lock_guard<TcpManager>(tcp_manager_));
      }
    } else {
      ;
    }
    
    tcp_manager_.SwapPacketsForSending(packets_for_sending,
        std::lock_guard<TcpManager>(tcp_manager_));
    for (auto &packet : packets_for_sending) {
      char *begin = nullptr, *end = nullptr;
      std::tie(begin, end) = packet->GetBuffer();
      if (rand()%5 < 4) {
        sendto(host_socket, begin, static_cast<size_t>(end-begin), 0,
               (sockaddr *)&peer_addr_, sizeof(sockaddr_in));
        std::cerr << "Send packet" << std::endl;
      } else {
        std::cerr << "Packet lost" << std::endl;
      }
    }
    packets_for_sending.clear();
    
    std::cerr << std::endl;
  }
}

} // namespace tcp_simulator
