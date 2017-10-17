#ifndef _TCP_STACK_SAFE_LOG_H_
#define _TCP_STACK_SAFE_LOG_H_

#include <iostream>
#include <sstream>

namespace tcp_stack {
template <class... Args>
void Log(const Args&... args) {
  std::ostringstream output;
  (output << ... << args) << std::endl << std::flush;
  std::clog << output.rdbuf()->str();
}

} // namespace tcp_stack

#endif // _TCP_STACK_SAFE_LOG_H_
