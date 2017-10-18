
#include <iostream>
#include <cstring>

#include "socket-manager.h"

using namespace tcp_stack;

// TODO: test checksum failure
// TODO: test resend
// TODO: test resend canceling

// TODO: reply of unexpected packets
// TODO: network system

// TODO: overlap already received segement
// TODO: circular seq/ack number

// TODO: Add resend for GetPacketForSending

int main() {
  SocketManager server(1), client(2);

  std::thread th([&]() {
        auto server_socket = server.NewSocket();
        auto client_socket = client.NewSocket();

        server_socket.Listen(10);
        client_socket.Connect("0.0.0.1", 10);

        client_socket.Send("Abcdefghijklmno", 15);
        client_socket.Send("pqrstuvwxyz", 11);

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

  for (int i=0; i<40; ++i) {  
    auto packets = client.GetPacketsForSending();
    Log(packets.size());
    for (auto &packet : packets)
      server.ReceivePacket(packet);

    packets = server.GetPacketsForSending();
    Log(packets.size());
    for (auto &packet : packets)
      client.ReceivePacket(packet);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  Log("Network ended");

  th.join();
  Log("Main ended");

  std::this_thread::sleep_for(std::chrono::milliseconds(10000));

  return 0;
}
