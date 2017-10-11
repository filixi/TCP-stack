
#include <iostream>
#include <cstring>

#include "socket-manager.h"

using namespace tcp_stack;

// TODO: bug in closing stage
// TODO: port number allocation (std::set<Ip&Port>)
// TODO: nothrow close

int main() {
  SocketManager server(1), client(2);

  std::thread th([&]() {
        auto server_socket = server.NewSocket();
        auto client_socket = client.NewSocket();

        server_socket.Listen(10);
        client_socket.Connect("0.0.0.1", 10);

        client_socket.Send("Abcdefghijklmnopqrstuvwxyz", 26);

        char buff[27] = {0};
        auto server_connection = server_socket.Accept();
        server_connection.Recv(buff, 10);
        assert(!strcmp(buff, "Abcdefghij"));

        server_connection.Recv(buff, 16);
        assert(!strcmp(buff, "klmnopqrstuvwxyz"));

        // server_connection.Close();
        // client_socket.Close();
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

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "P" << std::endl;

  th.join();
  std::clog << "Main ended" << std::endl;

  return 0;
}
