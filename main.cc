
#include <vector>
#include <stdio.h>

#include "tcp.h"
#include "network.h"

#include <iostream>

int main() {
  auto service1 = NetworkService::AsyncRun(
      "127.0.0.1", 15259, "127.0.0.1", 15260);
  auto service2 = NetworkService::AsyncRun(
      "127.0.0.1", 15260, "127.0.0.1", 15259);
  
  auto sock1 = service1->NewTcpSocket(15);
  auto sock2 = service2->NewTcpSocket(25);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  
  sock1.Listen(15);
  sock2.Connect(15);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  auto sock3 = sock1.Accept();
  std::cerr << "Accepted" << std::endl;
  
  sock2.Close();
  
  service1->join();
  service2->join();
  
  return 0;
}
