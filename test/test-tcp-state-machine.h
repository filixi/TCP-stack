
#include <cassert>
#include <cstddef>

#include <string>
#include <vector>

#include "state.h"

using namespace tcp_stack;
using namespace std::literals::string_literals;

class TestInternal : public TcpInternalInterface {
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

void TestConnection() {
  // CLOSED -> SYNSENT -> ESTAB, CLOSED -> SYNRCVD -> ESTAB
  {
    TcpStateManager tcp1, tcp2;
    TestInternal internal1, internal2;
    
    tcp1(Event::kConnect, nullptr)(&internal1);
    assert(internal1[-1] == "SendSyn"s);
    assert(tcp1.GetState() == State::kSynSent);

    tcp2(internal1.GetHeader())(&internal2);
    assert(internal2[-1] == "SendSynAck"s);
    assert(internal2[-2] == "Accept"s);
    assert(tcp2.GetState() == State::kSynRcvd);

    tcp1(internal2.GetHeader())(&internal1);
    assert(internal1[-1] == "SendAck"s);
    assert(internal1[-2] == "Accept"s);
    assert(tcp1.GetState() == State::kEstab);

    tcp2(internal1.GetHeader())(&internal2);
    assert(internal2[-1] == "Accept"s);
    assert(tcp2.GetState() == State::kEstab);
  }
}

void test_tcp_state_machine() {
  TestConnection();
  std::clog << __func__ << " Passed" << std::endl;
}
