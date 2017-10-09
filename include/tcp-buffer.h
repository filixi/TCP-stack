#ifndef _TCP_STACK_TCP_BUFFER_H_
#define _TCP_STACK_TCP_BUFFER_H_

#include <cassert>

#include <deque>
#include <memory>

#include "tcp-header.h"

namespace tcp_stack {
class Buffer {
public:
  void PushBack(const char *source, size_t size) {
    buff_.insert(buff_.end(), source, source+size);
  }

  void PopFront(size_t size) {
    assert(size <= buff_.size());
    buff_.erase(buff_.begin(), buff_.begin() + size);
  }

  size_t Size() const {
    return buff_.size();
  }

  bool Empty() const {
    return buff_.size() == 0;
  }

  void Get(char *sink, uint32_t first, uint32_t last) {
    std::copy(buff_.begin() + first, buff_.begin() + last, sink);
  }

  void Clear() {
    buff_.clear();
  }

private:
  std::deque<char> buff_;
};

class TcpSendingBuffer {
public:
  void InitializeAckNumber(uint32_t ack) {
    last_ack_ = ack;
  }

  void Ack(uint32_t ack) {
    // TODO: add support to circle ack number.
    assert(ack >= last_ack_);
    if (ack - last_ack_ <= Size()) {
      buff_.PopFront(ack - last_ack_);
      last_ack_ = ack;
    }
  }

  void Push(const char *source, size_t size) {
    buff_.PushBack(source, size);
  }

  void Get(char *sink, uint32_t first, uint32_t last) {
    assert(first >= 0);
    assert(first <= last);
    assert(last < Size());

    buff_.Get(sink, first, last);
  }

  auto GetAsTcpPacket(uint32_t first, uint32_t last) {
    auto tcp_packet = MakeTcpPacket(last-first);
    Get(tcp_packet->begin(), first, last);
    return tcp_packet;
  }

  bool Empty() const {
    return buff_.Empty();
  }

  size_t Size() const {
    return buff_.Size();
  }

  void Clear() {
    buff_.Clear();
  }
  
private:
  Buffer buff_;
  uint32_t last_ack_;
};


} // namespace tcp_stack

#endif // _TCP_STACK_TCP_BUFFER_H_
