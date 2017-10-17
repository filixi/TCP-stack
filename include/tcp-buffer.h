#ifndef _TCP_STACK_TCP_BUFFER_H_
#define _TCP_STACK_TCP_BUFFER_H_

#include <cassert>
#include <cstddef>

#include <deque>
#include <memory>

#include "safe-log.h"
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
    initial_ack_ = last_ack_ = ack;
  }

  void Ack(uint32_t ack) {
    // TODO: add support to circle ack number.
    assert(ack >= last_ack_);

    Log("Buffer_ACK", ack, " ", last_ack_, " ", last_get_);

    if (ack - last_ack_ <= Size()) {
      last_get_ -= ack - last_ack_;
      buff_.PopFront(ack - last_ack_);
      last_ack_ = ack;
    } else {
      last_get_ = 0;
      buff_.Clear();
      last_ack_ = ack;
    }

    Log("Buff Size:", Size());
  }

  void Push(const char *source, size_t size) {
    buff_.PushBack(source, size);
  }

  void Get(char *sink, uint32_t first, uint32_t last) {
    assert(first >= 0);
    assert(first <= last);
    assert(last <= Size());

    buff_.Get(sink, first, last);
  }

  auto GetAsTcpPacket(uint32_t first, uint32_t last) {
    const uint32_t abs_first = first + last_get_;
    uint32_t abs_last = last + last_get_;

    assert(abs_first <= Size());

    if (abs_last >= Size())
      abs_last = Size();

    Log("A packet is retreived ");
    auto tcp_packet = MakeTcpPacket(abs_last - abs_first);
    Get(tcp_packet->begin(), abs_first, abs_last);
    tcp_packet->GetHeader().TcpLength() = abs_last - abs_first;

    last_get_ = abs_last;
    return tcp_packet;
  }

  bool Empty() const {
    Log("Emtpy", " ", Size(), " ", last_get_);
    return static_cast<ptrdiff_t>(Size()) == last_get_;
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
  uint32_t initial_ack_;

  int64_t last_get_ = 0;
};

} // namespace tcp_stack

#endif // _TCP_STACK_TCP_BUFFER_H_
