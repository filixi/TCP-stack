#include "tcp-header.h"

namespace tcp_stack {
std::ostream &operator<<(std::ostream &o, const TcpHeader &header) {
  if (header.Ack())
    o << "Ack ";
  if (header.Syn())
    o << "Syn ";
  if (header.Rst())
    o << "Rst ";
  if (header.Fin())
    o << "Fin ";
  
  o << header.SourcePort() << "->" << header.DestinationPort() << " "
    << "S" << header.SequenceNumber() << " "
    << "A" << header.AcknowledgementNumber() << " ";
  return o;
}

} // namespace tcp_stack
