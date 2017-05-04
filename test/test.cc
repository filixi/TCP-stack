#define private public

#include <stdexcept>
#include <vector>

#include "tcp-state-machine.h"

using namespace tcp_simulator;

void TestClosed2SynRcvd();
void TestClosed2SynSent();
void TestClosedError();

int main() {
  TestClosed2SynRcvd();
  TestClosed2SynSent();
  TestClosedError();
  return 0;
}

struct TestInternal : TcpInternalInterface {
  void SendAck() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::SendAck);
  }
  
  void SendConditionAck() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::SendConditionAck);
  }
  
  void SendSyn() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::SendSyn);
  }
  
  void SendSynAck() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::SendSynAck);
  }
  
  void SendFin() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::SendFin);
  }
  
  void SendRst() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::SendRst);
  }
  
  void Accept() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::Accept);
  }
  
  void Discard() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::Discard);
  }
  
  void Close() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::Close);
  }
  
  void NewConnection() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::NewConnection);
  }
  
  void Reset() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::Reset);
  }
  
  void NotifyOnConnected() {}
  void NotifyOnReceivingMessage() {}
  void NotifyOnNewConnection(size_t) {}

  using FunctionPtr = void (TestInternal::*)();
  
  TestInternal &AssertCalled(FunctionPtr ptr, const std::string &message) {
    if (std::find(member_called_.begin(), member_called_.end(), ptr) ==
        member_called_.end())
      throw std::runtime_error(message);
    return *this;
  }
  
  TestInternal &AssertOnlyOneCall() {
    if (member_called_.size() != 1)
      throw std::runtime_error("One Call Failed");
    return *this;
  }
  
  void Clear() {
    member_called_.clear();
  }
  
  std::vector<FunctionPtr> member_called_;
};

struct TestHeaderFactory {
  TcpHeader AckHeader(uint32_t seq, uint32_t ack, uint16_t window) {
    TcpHeader header;
    header.SequenceNumber() = seq;
    header.AcknowledgementNumber() = ack;
    header.Window() = window;
    header.SetAck(true);
    return header;
  }
  
  TcpHeader NextAck(const TcpStateMachine &machine) {
    return AckHeader(machine.host_last_ack_, machine.host_next_seq_, 10240);
  }
  
  TcpHeader SynHeader(uint32_t seq, uint16_t window) {
    TcpHeader header;
    header.SetSyn(true);
    header.SequenceNumber() = seq;
    header.Window() = window;
    return header;
  }
  
  TcpHeader SynAckHeader(const TcpStateMachine &machine, uint32_t seq,
                         uint16_t window) {
    return AckHeader(seq, machine.host_next_seq_, window);
  }
  
  TcpHeader RstHeader(uint32_t seq) {
    TcpHeader header;
    header.SequenceNumber() = seq;
    header.SetRst(true);
    return header;
  }
  
  TcpHeader FinHeader(uint32_t seq) {
    TcpHeader header;
    header.SequenceNumber() = seq;
    header.SetFin(true);
    return header;
  }
  
  TcpHeader FinHeader(const TcpStateMachine &machine) {
    return FinHeader(machine.host_last_ack_);
  }
};

// After recevice Syn, the state should move to the state after sending SynAck
void TestClosed2SynRcvd() {
  std::cerr << __func__ << std::endl;
  
  TcpStateMachine machine;
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  auto react = machine.OnReceivePacket(&header, 0);
  react(&internal);
  
  internal.AssertCalled(&TestInternal::Accept, "TestClosed Accept")
          .AssertCalled(&TestInternal::SendSynAck, "TestClosed SendSynAck");
  
  assert(machine.GetState() == State::kSynRcvd);
  assert(machine.peer_window_ == 10240);
  assert(machine.peer_last_ack_ == machine.host_initial_seq_);
  
  assert(machine.peer_initial_seq_ == 17);
  assert(machine.host_last_ack_ == machine.peer_initial_seq_ + 1); // next ack
  
  // Syn's seq isn't from next_seq, this is for ack checking
  assert(machine.host_next_seq_ == machine.host_initial_seq_ + 1);
}

// After sending syn, the peer_last_ack should be ready for checking the next
// peer's ack 
void TestClosed2SynSent() {
  std::cerr << __func__ << std::endl;
  
  TcpStateMachine machine;
  
  machine.SynSent(1425, 10240); // seq window
  
  assert(machine.GetState() == State::kSynSent);
  assert(machine.host_window_ == 10240);
  assert(machine.peer_last_ack_ == machine.host_initial_seq_);
  
  assert(machine.host_initial_seq_ == 1425);
  assert(machine.host_next_seq_ == machine.host_initial_seq_+1);
}

void TestClosedError() {
  std::cerr << __func__ << std::endl;
  
  TcpStateMachine machine;
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.AckHeader(17, 17, 10240);
  auto react = machine.OnReceivePacket(&header, 0);
  react(&internal);
  internal.AssertCalled(&TestInternal::Discard, "TestClosed Discard1")
          .AssertOnlyOneCall()
          .Clear();
          
  header = factory.SynAckHeader(machine, 17, 10240);
  react = machine.OnReceivePacket(&header, 0);
  react(&internal);
  internal.AssertCalled(&TestInternal::Discard, "TestClosed Discard2")
          .AssertOnlyOneCall()
          .Clear();
  
  header = factory.FinHeader(12);
  react = machine.OnReceivePacket(&header, 0);
  react(&internal);
  internal.AssertCalled(&TestInternal::Discard, "TestClosed Discard2")
          .AssertOnlyOneCall()
          .Clear();
}