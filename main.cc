
#include <iostream>
#include <cstring>

#include "network-service.h"

using namespace tcp_stack;

// TODO: test resend
// TODO: test resend canceling

// TODO: reply of unexpected packets
// TODO: network system

// TODO: overlap already received segement
// TODO: circular seq/ack number

int main() {
  auto server = NetworkService::AsyncRun(
      "127.0.0.1", 15500, "127.0.0.1", 15501);
  auto client = NetworkService::AsyncRun(
      "127.0.0.1", 15501, "127.0.0.1", 15500);
  
  Log("Service up");
  std::thread th([&]() {
        auto server_socket = server->NewSocket();
        auto client_socket = client->NewSocket();

        server_socket.Listen(10);

        Log("Connecting");
        client_socket.Connect("127.0.0.1", 10);

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
  
  th.join();
  Log("Main ended");
  std::this_thread::sleep_for(std::chrono::seconds(10));
  return 0;
}
