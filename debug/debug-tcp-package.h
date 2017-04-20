
#include <algorithm>
#include <iostream>
#include <iterator>

#include "../tcp-buffer.h"

void DebugTcpPackage() {
  std::shared_ptr<NetworkPackage> ptr = NetworkPackage::NewPackage(30);
  TcpPackage package(ptr);
  
  new(&package.GetHeader()) TcpHeader();
  std::fill_n(package.GetData().first, 10, '7');
  
  package.GetHeader().Checksum() = package.CalculateChecksum();
  assert(package.CalculateChecksum() == 0);
  
  package.GetHeader().Window() = 0xff;

  std::cout << ptr << std::endl;
  std::cout << std::endl;
}