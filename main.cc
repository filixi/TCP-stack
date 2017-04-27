
#include "include/tcp.h"
#include "include/network.h"

#include <iostream>

using namespace tcp_simulator;

int main() {
  auto service1 = NetworkService::AsyncRun(
      "127.0.0.1", 15259, "127.0.0.1", 15260);
  auto service2 = NetworkService::AsyncRun(
      "127.0.0.1", 15260, "127.0.0.1", 15259);
  
  auto sock1 = service1->NewTcpSocket(15);
  auto sock2 = service2->NewTcpSocket(25);
  
  sock1.Listen(15);
  sock2.Connect(15);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  auto sock3 = sock1.Accept();
  std::cerr << "Accepted" << std::endl;
  
  std::this_thread::sleep_for(std::chrono::seconds(1));
  char buff[] = "abc";
  sock3.Write(std::begin(buff), std::end(buff));
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  auto data = sock2.Read();
  if (data.second)
    std::cout.write(data.first.front().GetData().first,
                    data.first.front().Length());
  
  std::this_thread::sleep_for(std::chrono::seconds(1));
  sock2.Close();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  service1->join();
  service2->join();
  
  return 0;
}
