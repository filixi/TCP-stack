#include "tcp-buffer.h"

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