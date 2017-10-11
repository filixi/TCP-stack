
#include <iostream>

#include "socket-manager.h"

using namespace tcp_stack;

int main() {
  SocketManager server(1), client(2);

  auto server_socket = server.NewSocket();
  auto client_socket = client.NewSocket();

  std::thread th([&server_socket, &client_socket]() {
        server_socket.Listen(10);
        client_socket.Connect("0.0.0.1", 10);

        std::clog << "connected" << std::endl;
      });
  std::this_thread::sleep_for(std::chrono::seconds(1));

  for (int i=0; i<20; ++i) {  
    auto packets = client.GetPacketsForSending();
    std::cout << packets.size() << std::endl;
    for (auto &packet : packets) {
      std::cout << packet->GetHeader() << std::endl;
      server.ReceivePacket(packet);
    }

    packets = server.GetPacketsForSending();
    std::cout << packets.size() << std::endl;
    for (auto &packet : packets) {
      std::cout << packet->GetHeader() << std::endl;
      client.ReceivePacket(packet);
    }
  }
  
  
  th.join();
  std::clog << "Main ended" << std::endl;

  return 0;
}
