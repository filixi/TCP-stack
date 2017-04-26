#include "tcp-buffer.h"

namespace tcp_simulator {

uint16_t TcpPackage::CalculateChecksum() {
  uint32_t checksum = 0;
  uint16_t * const buffer = reinterpret_cast<uint16_t *>(
      network_package_->GetBuffer().first);
  const auto size = network_package_->Length();
  size_t i = 0;
  for (; i<size/2; ++i)
    checksum += buffer[i];
  if (size%2)
    checksum += network_package_->GetBuffer().first[size-1];

  return static_cast<uint16_t>(~checksum);
}

std::ostream &operator<<(std::ostream &o,
                         const std::shared_ptr<NetworkPackage> &ptr) {
  auto range = ptr->GetBuffer();
  
  int ic = 0;
  while (range.first != range.second) {
    for (int i=0; i<8; ++i)
      o << ((*range.first&(1<<i))?'1' : '0');
    ++range.first;
    
    if (++ic%4==0)
      o << std::endl;
    else
      o << " ";
  }
  return o;
}

std::ostream &operator<<(std::ostream &o,
                         const TcpPackage &package) {
  const auto &header = package.GetHeader();
  if (header.Ack())
    o << "Ack ";
  if (header.Syn())
    o << "Syn ";
  if (header.Rst())
    o << "Rst ";
  if (header.Fin())
    o << "Fin ";
  
  o << header.SourcePort() << "->" << header.DestinationPort() << " ";
  o << "S" << header.SequenceNumber() << " ";
  o << "A" << header.AcknowledgementNumber();
  return o;
}

}
