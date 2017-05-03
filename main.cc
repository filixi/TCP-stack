
#include "include/tcp-socket.h"
#include "include/network.h"

#include <cassert>

#include <iostream>

using namespace tcp_simulator;

int main() {
  auto service1 = NetworkService::AsyncRun(
      "127.0.0.1", 15259, "127.0.0.1", 15260);
  auto service2 = NetworkService::AsyncRun(
      "127.0.0.1", 15260, "127.0.0.1", 15259);
  
  auto sock1 = service1->NewTcpSocket(1);
  auto sock2 = service2->NewTcpSocket(100);
  
  sock1.Listen(1);
  sock2.Connect(1);

  auto sock3 = sock1.Accept();
  std::cerr << "Accepted" << std::endl;

  char buff[] = "\
I found out that the short message - abc - is very hard to be \
found among all those annoying logs!!!";

  sock3.Write(std::begin(buff), std::end(buff));

  auto data = sock2.Read();
  assert(data.second);
  
  std::cerr << data.first.front().GetData().first << std::endl;

  

  service1->join();
  service2->join();
  
  sock2.Close();
  
  return 0;
}
