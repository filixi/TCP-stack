
#include <cassert>
#include <cstddef>

#include <string>
#include <vector>

#include "state.h"

using namespace tcp_stack;
using namespace std::literals::string_literals;

class TestInternal : public SocketInternalInterface {
public:
  void SendSyn(uint32_t seq, uint16_t window) override {
    called_.push_back(__func__);
    header_ = TcpHeader();
    header_.SetSyn(true);
    header_.SequenceNumber() = seq;
    header_.Window() = window;
  }

  void SendSynAck(uint32_t seq, uint32_t ack, uint16_t window) override {
    called_.push_back(__func__);
    header_ = TcpHeader();
    header_.SetSyn(true);
    header_.SetAck(true);
    header_.SequenceNumber() = seq;
    header_.AcknowledgementNumber() = ack;
    header_.Window() = window;
  }

  void SendAck(uint32_t seq, uint32_t ack, uint16_t window) override {
    called_.push_back(__func__);
    header_ = TcpHeader();
    header_.SetAck(true);
    header_.SequenceNumber() = seq;
    header_.AcknowledgementNumber() = ack;
    header_.Window() = window;
  }
  
  void SendFin(uint32_t seq, uint32_t ack, uint16_t window) override {
    called_.push_back(__func__);
    header_ = TcpHeader();
    header_.SetAck(true);
    header_.SetFin(true);
    header_.SequenceNumber() = seq;
    header_.AcknowledgementNumber() = ack;
    header_.Window() = window;
  }

  void RecvSyn(uint32_t seq_recv, uint16_t window_recv) override {
    called_.push_back(__func__);
  }

  void RecvAck(
      uint32_t seq_recv, uint32_t ack_recv, uint16_t window_recv) override {
    called_.push_back(__func__);
  }

  void RecvFin(
      uint32_t seq_recv, uint32_t ack_recv, uint16_t window_recv) override {
    called_.push_back(__func__);
  }

  void Listen() override {
    called_.push_back(__func__);
  }

  void Connected() override {
    called_.push_back(__func__);
  }

  void Accept() override {
    called_.push_back(__func__);
  }
  
  void Discard() override {
    called_.push_back(__func__);
  }
  
  void SeqOutofRange(uint16_t window) override {
    called_.push_back(__func__);
  }
  
  void SendRst(uint32_t seq) override {
    called_.push_back(__func__);
  }

  void InvalidOperation() override {
    called_.push_back(__func__);
  }

  void NewConnection() override {
    called_.push_back(__func__);
  }

  void Close() override {
    called_.push_back(__func__);
  }

  const auto &GetCalled() const {
    return called_;
  }
  
  const auto &GetLastCalled() const {
    return called_.back();
  }

  const auto &operator[](std::ptrdiff_t index) const {
    if (index >= 0)
      return called_.at(index);
    else
      return called_.at(called_.size() + index);
  }

  const TcpHeader &GetHeader() const {
    return header_;
  }

private:
  std::vector<std::string> called_;
  TcpHeader header_;
};

auto GetReturn(std::function<void(SocketInternalInterface *)> react,
               TcpStateManager tcp) {
  return std::function([=](SocketInternalInterface *internal) mutable {
        react(internal);
        return tcp;
      });
}

auto TestConnection(State state = State::kClosed) {
  // tcp1 : CLOSED -> SYNSENT -> ESTAB -> CLOSEWAIT -> LAST-ACK -> CLOSED
  // tcp2 : CLOSED -> SYNRCVD -> ESTAB -> FINWAIT-1 -> FINWAIT-2 -> TIMEWAIT

  TcpStateManager tcp1, tcp2;
  TestInternal internal1, internal2;

  auto react = tcp1(Event::kConnect, nullptr);
  if (state == State::kSynSent)
    return GetReturn(react, tcp1);
  react(&internal1);
  assert(internal1[-1] == "SendSyn"s);
  assert(tcp1.GetState() == State::kSynSent);

  react = tcp2(internal1.GetHeader());
  if (state == State::kSynRcvd)
    return GetReturn(react, tcp2);
  react(&internal2);
  assert(internal2[-1] == "SendSynAck"s);
  assert(internal2[-2] == "Accept"s);
  assert(tcp2.GetState() == State::kSynRcvd);

  react = tcp1(internal2.GetHeader());
  if (state == State::kEstab)
    return GetReturn(react, tcp1);
  react(&internal1);
  assert(internal1[-1] == "Connected"s);
  assert(internal1[-2] == "SendAck"s);
  assert(internal1[-3] == "Accept"s);
  assert(tcp1.GetState() == State::kEstab);
  
  // tcp1 : ESTAB -> CLOSEWAIT/FINWAIT-1 ...
  // tcp2 : SYNRCVD -> FINWAIT-1 ...
  [=]() mutable {
    tcp2(Event::kClose, nullptr)(&internal2);
    assert(internal2[-1] == "SendFin"s);
    assert(tcp2.GetState() == State::kFinWait1);

    tcp2(internal1.GetHeader())(&internal2);
    assert(internal2[-1] == "Accept"s);
    assert(tcp2.GetState() == State::kFinWait1);

    // tcp1 : ESTAB -> FINWAIT-1 ...
    // tcp2 : FINWAIT1 -> CLOSING ...
    [=]() mutable {
      tcp1(Event::kClose, nullptr)(&internal1);
      assert(internal1[-1] == "SendFin"s);
      assert(tcp1.GetState() == State::kFinWait1);

      const auto tcp2_fin_header = internal2.GetHeader();

      tcp2(internal1.GetHeader())(&internal2);
      assert(internal2[-1] == "SendAck"s);
      assert(internal2[-2] == "Accept"s);
      assert(tcp2.GetState() == State::kClosing);

      tcp1(internal2.GetHeader())(&internal1);
      assert(internal1[-1] == "Accept"s);
      assert(tcp1.GetState() == State::kFinWait2);

      tcp1(tcp2_fin_header)(&internal1);
      assert(internal1[-1] == "SendAck"s);
      assert(internal1[-2] == "Accept"s);
      assert(tcp1.GetState() == State::kTimeWait);

      tcp2(internal1.GetHeader())(&internal2);
      assert(internal2[-1] == "Accept"s);
      assert(tcp2.GetState() == State::kTimeWait);
    }();

    tcp1(internal2.GetHeader())(&internal1);
    assert(internal1[-1] == "SendAck"s);
    assert(internal1[-2] == "Accept"s);
    assert(tcp1.GetState() == State::kCloseWait);

    tcp2(internal1.GetHeader())(&internal2);
    assert(internal2[-1] == "Accept"s);
    assert(tcp2.GetState() == State::kFinWait2);

    tcp1(Event::kClose, nullptr)(&internal1);
    assert(internal1[-1] == "SendFin"s);
    assert(tcp1.GetState() == State::kLastAck);

    tcp2(internal1.GetHeader())(&internal2);
    assert(internal2[-1] == "SendAck"s);
    assert(internal2[-2] == "Accept"s);
    assert(tcp2.GetState() == State::kTimeWait);

    tcp1(internal2.GetHeader())(&internal1);
    assert(internal1[-1] == "Accept"s);
    assert(tcp1.GetState() == State::kClosed);
  }();

  tcp2(internal1.GetHeader())(&internal2);
  assert(internal2[-1] == "Connected"s);
  assert(internal2[-2] == "Accept"s);
  assert(tcp2.GetState() == State::kEstab);

  react = tcp2(Event::kClose, nullptr);
  if (state == State::kFinWait1)
    return GetReturn(react, tcp2);
  react(&internal2);
  assert(internal2[-1] == "SendFin"s);
  assert(tcp2.GetState() == State::kFinWait1);

  react = tcp1(internal2.GetHeader());
  if (state == State::kCloseWait)
    return GetReturn(react, tcp1);
  react(&internal1);
  assert(internal1[-1] == "SendAck"s);
  assert(internal1[-2] == "Accept"s);
  assert(tcp1.GetState() == State::kCloseWait);

  react = tcp2(internal1.GetHeader());
  if (state == State::kFinWait2)
    return GetReturn(react, tcp2);
  react(&internal2);
  assert(internal2[-1] == "Accept"s);
  assert(tcp2.GetState() == State::kFinWait2);

  react = tcp1(Event::kClose, nullptr);
  if (state == State::kLastAck)
    return GetReturn(react, tcp1);
  react(&internal1);
  assert(internal1[-1] == "SendFin"s);
  assert(tcp1.GetState() == State::kLastAck);

  react = tcp2(internal1.GetHeader());
  if (state == State::kTimeWait)
    return GetReturn(react, tcp2);
  react(&internal2);
  assert(internal2[-1] == "SendAck"s);
  assert(internal2[-2] == "Accept"s);
  assert(tcp2.GetState() == State::kTimeWait);

  tcp1(internal2.GetHeader())(&internal1);
  assert(internal1[-1] == "Accept"s);
  assert(tcp1.GetState() == State::kClosed);

  return GetReturn([](auto){}, TcpStateManager());
}

void test_tcp_state_machine() {
  TestConnection();
  std::clog << __func__ << " Passed" << std::endl;
}
