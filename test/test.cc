#define private public

#include <memory>
#include <stdexcept>
#include <vector>

#include "tcp-state-machine.h"

using 
namespace tcp_simulator;

std::shared_ptr<TcpStateMachine> TestClosed2SynRcvd();
std::shared_ptr<TcpStateMachine> TestClosed2SynSent();
void TestClosedError();

std::shared_ptr<TcpStateMachine> TestSynSent2SynRcvd();
std::shared_ptr<TcpStateMachine> TestSynSent2Estab();
void TestSynSentError();

std::shared_ptr<TcpStateMachine> TestSynRcvd2Estab();
std::shared_ptr<TcpStateMachine> TestSynRcvd2FinWait1();
void TestSynRcvdError();

std::shared_ptr<TcpStateMachine> TestEstab2Estab();
std::shared_ptr<TcpStateMachine> TestEstab2FinWait1();
std::shared_ptr<TcpStateMachine> TestEstab2CloseWait();
void TestEstabError();

std::shared_ptr<TcpStateMachine> TestFinWait12FinWait2();
std::shared_ptr<TcpStateMachine> TestFinWait12Closing();
void TestFinWait12Closed();
void TestFinWait1Error();

std::shared_ptr<TcpStateMachine> TestCloseWait2LastAck();
void TestCloseWaitError();

void TestFinWait22Closed();
void TestFinWait2Error();

void TestClosing2Closed();
void TestClosingError();

void TestLastAck2Closed();
void TestLastAckError();

int main() {
  TestClosed2SynRcvd();
  TestClosed2SynSent();
  TestClosedError();
  std::cerr << "----------------" << std::endl;
  
  TestSynSent2SynRcvd();
  TestSynSent2Estab();
  TestSynSentError();
  std::cerr << "----------------" << std::endl;
  
  TestSynRcvd2Estab();
  TestSynRcvd2FinWait1();
  TestSynRcvdError();
  std::cerr << "----------------" << std::endl;
  
  TestEstab2Estab();
  TestEstab2FinWait1();
  TestEstab2CloseWait();
  TestEstabError();
  std::cerr << "----------------" << std::endl;
  
  TestFinWait12FinWait2();
  TestFinWait12Closing();
  TestFinWait12Closed();
  TestFinWait1Error();
  std::cerr << "----------------" << std::endl;
  
  TestCloseWait2LastAck();
  TestCloseWaitError();
  std::cerr << "----------------" << std::endl;
  
  TestFinWait22Closed();
  TestFinWait2Error();
  std::cerr << "----------------" << std::endl;
  
  TestClosing2Closed();
  TestClosingError();
  std::cerr << "----------------" << std::endl;
  
  TestLastAck2Closed();
  TestLastAckError();
  std::cerr << "----------------" << std::endl;
  
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
  
  void NotifyOnConnected() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::NotifyOnConnected);
  }
  void NotifyOnReceivingMessage() {
    std::cerr << "!" << __func__ << std::endl;
    member_called_.push_back(&TestInternal::NotifyOnReceivingMessage);
  }
  void NotifyOnNewConnection(size_t) {
    std::cerr << "!" << __func__ << std::endl;
  }

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

struct TcpStateMachineCache {
  TcpStateMachineCache(TcpStateMachine &machine)
      : state_(machine.state_), stage_(machine.stage_),
        peer_initial_seq_(machine.peer_initial_seq_),
        peer_last_ack_(machine.peer_last_ack_),
        peer_window_(machine.peer_window_),
        host_initial_seq_(machine.host_initial_seq_),
        host_next_seq_(machine.host_next_seq_),
        host_last_ack_(machine.host_last_ack_),
        host_window_(machine.host_window_) {}
  
  bool operator==(const TcpStateMachineCache &rhs) {
    return state_ == state_ && stage_ == stage_ &&
           peer_initial_seq_ == peer_initial_seq_ &&
           peer_last_ack_ == peer_last_ack_ &&
           peer_window_ == peer_window_ &&
           host_initial_seq_ == host_initial_seq_ &&
           host_next_seq_ == host_next_seq_ &&
           host_last_ack_ == host_last_ack_ &&
           host_window_ == host_window_;
  }
  
  const State state_;
  const Stage stage_;
  
  const uint32_t peer_initial_seq_;
  const uint32_t peer_last_ack_;
  const uint32_t peer_window_;
  
  const uint32_t host_initial_seq_;
  const uint32_t host_next_seq_;
  const uint32_t host_last_ack_;
  const uint16_t host_window_;
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
  
  TcpHeader PeerAck(const TcpStateMachine &machine) {
    return AckHeader(machine.host_last_ack_, machine.host_next_seq_,
                     machine.peer_window_);
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
    auto header =  AckHeader(seq, machine.host_next_seq_, window);
    header.SetSyn(true);
    return header;
  }
  
  TcpHeader RstHeader(uint32_t seq) {
    TcpHeader header;
    header.SequenceNumber() = seq;
    header.SetRst(true);
    return header;
  }
  
  TcpHeader FinHeader(uint32_t seq, uint16_t ack, uint16_t window) {
    TcpHeader header;
    header.SequenceNumber() = seq;
    header.AcknowledgementNumber() = ack;
    header.Window() = window;
    header.SetFin(true);
    header.SetAck(true);
    return header;
  }
  
  TcpHeader FinHeader(const TcpStateMachine &machine) {
    return FinHeader(machine.host_last_ack_, machine.host_next_seq_,
                     machine.peer_window_);
  }
};

// After recevice Syn, the state should move to the state after sending SynAck
std::shared_ptr<TcpStateMachine> TestClosed2SynRcvd() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = std::make_shared<TcpStateMachine>();
  auto &machine = *machine_ptr;
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Accept, "Closed Accept")
          .AssertCalled(&TestInternal::SendSynAck, "Closed SendSynAck")
          .Clear();
  
  assert(machine.state_ == State::kSynRcvd);
  
  assert(machine.GetState() == State::kSynRcvd);
  assert(machine.peer_window_ == 10240);
  assert(machine.peer_last_ack_ == machine.host_initial_seq_);
  
  assert(machine.peer_initial_seq_ == 17);
  assert(machine.host_last_ack_ == machine.peer_initial_seq_ + 1); // next ack
  
  // Syn's seq isn't from next_seq, this is for ack checking
  assert(machine.host_next_seq_ == machine.host_initial_seq_ + 1);
  
  return machine_ptr;
}

// After sending syn, the peer_last_ack should be ready for checking the next
// peer's ack 
std::shared_ptr<TcpStateMachine> TestClosed2SynSent() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = std::make_shared<TcpStateMachine>();
  auto &machine = *machine_ptr;
  
  machine.SynSent(1425, 10240); // seq window
  
  assert(machine.state_ == State::kSynSent);
  
  assert(machine.GetState() == State::kSynSent);
  assert(machine.host_window_ == 10240);
  assert(machine.peer_last_ack_ == machine.host_initial_seq_);
  
  assert(machine.host_initial_seq_ == 1425);
  assert(machine.host_next_seq_ == machine.host_initial_seq_+1);
  
  return machine_ptr;
}

void TestClosedError() {
  std::cerr << "  " << __func__ << std::endl;
  
  TcpStateMachine machine;
  TestInternal internal;
  TestHeaderFactory factory;
  
  TcpStateMachineCache cache1(machine);
  
  auto header = factory.PeerAck(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Discard, "Closed Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache1 == TcpStateMachineCache(machine));
          
  header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "Closed Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache1 == TcpStateMachineCache(machine));
  
  header = factory.FinHeader(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "Closed Discard3")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache1 == TcpStateMachineCache(machine));
}

// syn rcvd, ack sent
std::shared_ptr<TcpStateMachine> TestSynSent2SynRcvd() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestClosed2SynSent();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Accept, "SynSent Accept")
          .AssertCalled(&TestInternal::SendAck, "SynSent SendAck")
          .Clear();

  assert(machine.state_ == State::kSynRcvd);
  
  assert(machine.peer_initial_seq_ == 17);
  assert(machine.peer_last_ack_ == cache.peer_last_ack_);
  assert(machine.peer_window_ == 10240);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_window_ == cache.host_window_);
  
  assert(machine.host_next_seq_ == machine.host_initial_seq_ + 1);
  assert(machine.host_last_ack_ == machine.peer_initial_seq_ + 1);
  
  return machine_ptr;
}

std::shared_ptr<TcpStateMachine> TestSynSent2Estab() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestClosed2SynSent();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Accept, "SynSent Accept")
          .AssertCalled(&TestInternal::SendAck, "SynSent SendAck")
          .Clear();
  
  assert(machine.state_ == State::kEstab);
  
  assert(machine.peer_initial_seq_ == 17);
  assert(machine.peer_last_ack_ == machine.host_initial_seq_ + 1);
  assert(machine.peer_window_ == 10240);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_next_seq_ == machine.host_initial_seq_ + 1);
  assert(machine.host_last_ack_ == machine.peer_initial_seq_ + 1);
  assert(machine.host_window_ == cache.host_window_);
  
  return machine_ptr;
}

void TestSynSentError() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestClosed2SynSent();
  auto &machine = *machine_ptr;
  TestInternal internal;
  TestHeaderFactory factory;
  
  TcpStateMachineCache cache1(machine);
  
  auto header = factory.PeerAck(machine);
  auto react = machine.OnReceivePacket(&header, 0);
  react(&internal);
  internal.AssertCalled(&TestInternal::Discard, "SynSent Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache1 == TcpStateMachineCache(machine));
  
  header = factory.FinHeader(machine);
  react = machine.OnReceivePacket(&header, 0);
  react(&internal);
  internal.AssertCalled(&TestInternal::Discard, "SynSent Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache1 == TcpStateMachineCache(machine));
}

std::shared_ptr<TcpStateMachine> TestSynRcvd2Estab() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestClosed2SynRcvd();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.PeerAck(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::NotifyOnConnected,
                        "SynRcvd NotifyOnConnected")
          .AssertOnlyOneCall()
          .Clear();
  
  assert(machine.state_ == State::kEstab);
  
  assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
  assert(machine.peer_last_ack_ == machine.host_initial_seq_ + 1);
  assert(machine.peer_window_ == cache.peer_window_);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_next_seq_ == cache.host_next_seq_);
  assert(machine.host_last_ack_ == cache.host_last_ack_);
  assert(machine.host_window_ == cache.host_window_);
  
  return machine_ptr;
}

std::shared_ptr<TcpStateMachine> TestSynRcvd2FinWait1() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestClosed2SynRcvd();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);

  machine.FinSent();
  
  assert(machine.state_ == State::kFinWait1);
  
  assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
  assert(machine.peer_last_ack_ == cache.peer_last_ack_);
  assert(machine.peer_window_ == cache.peer_window_);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_next_seq_ == cache.host_next_seq_);
  assert(machine.host_last_ack_ == cache.host_last_ack_);
  assert(machine.host_window_ == cache.host_window_);
  
  return machine_ptr;
}

void TestSynRcvdError() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestClosed2SynRcvd();
  auto &machine = *machine_ptr;
  TestInternal internal;
  TestHeaderFactory factory;
  
  TcpStateMachineCache cache(machine);
  
  auto header = factory.SynHeader(1024, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "SynRcvd Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
     
  header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "SynRcvd Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
  
  header = factory.FinHeader(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "SynRcvd Discard3")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
}

std::shared_ptr<TcpStateMachine> TestEstab2Estab() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestSynRcvd2Estab();
  auto &machine = *machine_ptr;

  TestInternal internal;
  TestHeaderFactory factory;
  
  std::cerr << "ack with no length" << std::endl;
  // ack with no length
  {
    TcpStateMachineCache cache(machine);
    auto header = factory.PeerAck(machine);
    machine.OnReceivePacket(&header, 0)(&internal);
    
    internal.AssertCalled(&TestInternal::SendConditionAck,
                          "TestEstab SendConditionAck")
            .AssertCalled(&TestInternal::Accept, "Estab Accept")
            .Clear();
    
    assert(machine.state_ == State::kEstab);
    
    assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
    assert(machine.peer_last_ack_ == header.AcknowledgementNumber());
    assert(machine.peer_window_ == header.Window());
    
    assert(machine.host_initial_seq_ == cache.host_initial_seq_);
    assert(machine.host_next_seq_ == cache.host_next_seq_);
    assert(machine.host_last_ack_ == cache.host_last_ack_);
    assert(machine.host_window_ == cache.host_window_);
  }
  
  std::cerr << "ack with length in order" << std::endl;
  // ack with length in order
  {
    TcpStateMachineCache cache(machine);
    auto header = factory.PeerAck(machine);
    machine.OnReceivePacket(&header, 20)(&internal);
    
    internal.AssertCalled(&TestInternal::SendConditionAck,
                          "TestEstab SendConditionAck")
            .AssertCalled(&TestInternal::Accept, "Estab Accept")
            .Clear();
    
    machine.OnSequencePacket(header, 20);
    
    assert(machine.state_ == State::kEstab);
    
    assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
    assert(machine.peer_last_ack_ == header.AcknowledgementNumber());
    assert(machine.peer_window_ == header.Window());
    
    assert(machine.host_initial_seq_ == cache.host_initial_seq_);
    assert(machine.host_next_seq_ == cache.host_next_seq_);
    assert(machine.host_last_ack_ == cache.host_last_ack_ + 20);
    assert(machine.host_window_ == cache.host_window_);
  }
  
  std::cerr << "ack with length out of order" << std::endl;
  // ack with length out of order
  {
    TcpStateMachineCache cache(machine);
    auto header = factory.PeerAck(machine);
    
    machine.OnReceivePacket(&header, 20)(&internal);
    ++header.SequenceNumber();
    internal.AssertCalled(&TestInternal::SendConditionAck,
                          "TestEstab SendConditionAck")
            .AssertCalled(&TestInternal::Accept, "Estab Accept")
            .Clear();
    
    machine.OnSequencePacket(header, 20);
    
    assert(machine.state_ == State::kEstab);
    
    assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
    assert(machine.peer_last_ack_ == header.AcknowledgementNumber());
    assert(machine.peer_window_ == header.Window());
    
    assert(machine.host_initial_seq_ == cache.host_initial_seq_);
    assert(machine.host_next_seq_ == cache.host_next_seq_);
    assert(machine.host_last_ack_ == cache.host_last_ack_);
    assert(machine.host_window_ == cache.host_window_);
  }
  
  return machine_ptr;
}

std::shared_ptr<TcpStateMachine> TestEstab2FinWait1() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestSynRcvd2Estab();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);

  machine.FinSent();
  ++machine.host_next_seq_;
  
  assert(machine.state_ == State::kFinWait1);
  
  assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
  assert(machine.peer_last_ack_ == cache.peer_last_ack_);
  assert(machine.peer_window_ == cache.peer_window_);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_next_seq_ == cache.host_next_seq_ + 1);
  assert(machine.host_last_ack_ == cache.host_last_ack_);
  assert(machine.host_window_ == cache.host_window_);
  
  return machine_ptr;
}

// and send ack
std::shared_ptr<TcpStateMachine> TestEstab2CloseWait() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestEstab2Estab();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.FinHeader(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Accept, "Estab Accept")
          .AssertCalled(&TestInternal::SendFin, "Estab Fin")
          .Clear();
  
  assert(machine.state_ == State::kCloseWait);
  
  assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
  assert(machine.peer_last_ack_ == cache.peer_last_ack_);
  assert(machine.peer_window_ == cache.peer_window_);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_next_seq_ == cache.host_next_seq_);
  assert(machine.host_last_ack_ == cache.host_last_ack_ + 1);
  assert(machine.host_window_ == cache.host_window_);
  
  return machine_ptr;
}

void TestEstabError() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestEstab2Estab();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Discard, "Estab Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
          
  header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "Estab Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
}

std::shared_ptr<TcpStateMachine> TestFinWait12FinWait2() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestEstab2FinWait1();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;

  auto header = factory.PeerAck(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Discard, "FinWait1 Discard")
          .AssertOnlyOneCall()
          .Clear();
  
  assert(machine.state_ == State::kFinWait2);

  assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
  assert(machine.peer_last_ack_ == header.AcknowledgementNumber());
  assert(machine.peer_window_ == cache.peer_window_);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_next_seq_ == cache.host_next_seq_);
  assert(machine.host_last_ack_ == cache.host_last_ack_);
  assert(machine.host_window_ == cache.host_window_);
  
  return machine_ptr;
}

std::shared_ptr<TcpStateMachine> TestFinWait12Closing() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestEstab2FinWait1();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.FinHeader(machine);
  --header.AcknowledgementNumber();
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Accept, "FinWait1 Accept")
          .AssertCalled(&TestInternal::SendFin, "FinWait1 SendFin")
          .Clear();
  
  assert(machine.state_ == State::kClosing);
  
  assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
  assert(machine.peer_last_ack_ == cache.peer_last_ack_);
  assert(machine.peer_window_ == cache.peer_window_);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_next_seq_ == cache.host_next_seq_);
  assert(machine.host_last_ack_ == cache.host_last_ack_ + 1);
  assert(machine.host_window_ == cache.host_window_);
  
  return machine_ptr;
}

void TestFinWait12Closed() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestEstab2FinWait1();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.FinHeader(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Close, "FinWait1 Accept")
          .AssertOnlyOneCall()
          .Clear();
  
  assert(machine.state_ == State::kClosed);
}

void TestFinWait1Error() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestEstab2FinWait1();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);

  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Discard, "FinWait1 Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
          
  header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "FinWait1 Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
  
  header = factory.PeerAck(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "FinWait1 Discard3")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
}

std::shared_ptr<TcpStateMachine> TestCloseWait2LastAck() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestEstab2CloseWait();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);

  machine.FinSent();
  ++machine.host_next_seq_;
  
  assert(machine.state_ == State::kLastAck);
  
  assert(machine.peer_initial_seq_ == cache.peer_initial_seq_);
  assert(machine.peer_last_ack_ == cache.peer_last_ack_);
  assert(machine.peer_window_ == cache.peer_window_);
  
  assert(machine.host_initial_seq_ == cache.host_initial_seq_);
  assert(machine.host_next_seq_ == cache.host_next_seq_ + 1);
  assert(machine.host_last_ack_ == cache.host_last_ack_);
  assert(machine.host_window_ == cache.host_window_);
  
  return machine_ptr;
}

void TestCloseWaitError() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestEstab2CloseWait();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);

  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Discard, "CloseWait Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
          
  header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "CloseWait Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
  
  header = factory.PeerAck(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "CloseWait Discard3")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
}

void TestFinWait22Closed() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestFinWait12FinWait2();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.FinHeader(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Close, "FinWait2 Accept")
          .AssertCalled(&TestInternal::SendAck, "FinWait2 Ack")
          .Clear();
  
  assert(machine.state_ == State::kClosed);
}

void TestFinWait2Error() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestFinWait12FinWait2();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);

  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Discard, "FinWait2 Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
          
  header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "FinWait2 Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
}

void TestClosing2Closed() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestFinWait12Closing();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;

  auto header = factory.PeerAck(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Close, "Closing Close")
          .AssertOnlyOneCall()
          .Clear();
  
  assert(machine.state_ == State::kClosed);
}

void TestClosingError() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestFinWait12Closing();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);

  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Discard, "Closing Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
          
  header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "Closing Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
}

void TestLastAck2Closed() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestCloseWait2LastAck();
  std::cerr << "Machine Initialized" << std::endl;
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);
  
  TestInternal internal;
  TestHeaderFactory factory;

  auto header = factory.PeerAck(machine);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Close, "LastAck Close")
          .AssertOnlyOneCall()
          .Clear();
  
  assert(machine.state_ == State::kClosed);
}

void TestLastAckError() {
  std::cerr << "  " << __func__ << std::endl;
  
  auto machine_ptr = TestCloseWait2LastAck();
  auto &machine = *machine_ptr;
  TcpStateMachineCache cache(machine);

  TestInternal internal;
  TestHeaderFactory factory;
  
  auto header = factory.SynHeader(17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  
  internal.AssertCalled(&TestInternal::Discard, "LastAck Discard1")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
          
  header = factory.SynAckHeader(machine, 17, 10240);
  machine.OnReceivePacket(&header, 0)(&internal);
  internal.AssertCalled(&TestInternal::Discard, "LastAck Discard2")
          .AssertOnlyOneCall()
          .Clear();
  assert(cache == TcpStateMachineCache(machine));
}