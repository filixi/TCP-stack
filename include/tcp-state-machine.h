#ifndef _TCP_STATE_MACHINE_H_
#define _TCP_STATE_MACHINE_H_

#include <cassert>

#include <functional>
#include <iostream>
#include <tuple>
#include <utility>
#include <vector>

#include "tcp-buffer.h"

namespace tcp_simulator {

enum class State{
  kClosed = 0,
  kListen,
  kSynRcvd,
  kSynSent,
  kEstab,
  kFinWait1,
  kCloseWait,
  kFinWait2,
  kClosing,
  kLastAck,
  kTimeWait,
};

enum class Stage {
  kClosed = 0,
  kHandShake,
  kEstab,
  kClosing
};

enum class Event {
  kNone = 0,
  kSynRecv,
  kAckRecv,
  kSynAckRecv,
  kFinRecv,
  kFinAckRecv,
  
  kFinSent,
  kSynSent,
  
  kRstRecv
};

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, State state) {
  static const std::vector<std::string> state_string {
    "kClosed",
    "kListen",
    "kSynRcvd",
    "kSynSent",
    "kEstab",
    "kFinWait1",
    "kCloseWait",
    "kFinWait2",
    "kClosing",
    "kLastAck",
    "kTimeWait"
  };
  
  return o << state_string.at(static_cast<int>(state));
}

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, Event event) {
  
  static const std::vector<std::string> event_string{
    "kNone",
    "kSynRecv",
    "kAckRecv",
    "kSynAckRecv",
    "kFinRecv",
    "kFinAckRecv",
    "kFinSend",
    "kSynSent",
    "kRstRecv"
  };
  
  return o << event_string.at(static_cast<int>(event));
}

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, Stage stage) {
  
  static const std::vector<std::string> stage_string{
    "kClosed",
    "kHandShake",
    "kEstab",
    "kClosing"
  };
  
  return o << stage_string.at(static_cast<int>(stage));
}

class HeaderFactory {
 public:
  HeaderFactory() = default;
  virtual ~HeaderFactory() = default;
  
  TcpPackage RstHeader(const TcpPackage &source) {
    TcpPackage package(nullptr, nullptr);
    const auto &source_header = source.GetHeader();
    auto &header = package.GetHeader();
    
    header.SourcePort() = source_header.DestinationPort();
    header.DestinationPort() = source_header.SourcePort();
    
    header.SequenceNumber() = source_header.AcknowledgementNumber() + 1;
    header.SetRst(true);
    
    header.Checksum() = 0;
    header.Checksum() = package.CalculateChecksum();
    return package;
  }
  
 private:
};

class TcpInternalInterface {
 public:
  virtual void SendAck() = 0;
  virtual void SendConditionAck() = 0;
  virtual void SendSyn() = 0;
  virtual void SendSynAck() = 0;
  virtual void SendFin() = 0;
  virtual void SendRst() = 0;
  virtual void Accept() = 0;
  virtual void Discard() = 0;
  virtual void Close() = 0;
  virtual void NewConnection() = 0;
  virtual void Reset() = 0;
  
  virtual ~TcpInternalInterface() = default;
};

class PuppyTcpInternal : public TcpInternalInterface {
 public:
  void SendAck() override {
    std::cerr << __func__ << std::endl;
  }
  
  void SendConditionAck() override {
    std::cerr << __func__ << std::endl;
  }
  
  void SendSyn() override {
    std::cerr << __func__ << std::endl;
  }
  void SendSynAck() override {
    std::cerr << __func__ << std::endl;
  }
  
  void SendFin() override {
    std::cerr << __func__ << std::endl;
  }
  
  void SendRst() override {
    std::cerr << __func__ << std::endl;
  }
  
  void Accept() override {
    std::cerr << __func__ << std::endl;
  }
  
  void Discard() override {
    std::cerr << __func__ << std::endl;
  }
  
  void Close() override {
    std::cerr << __func__ << std::endl;
  }
  
  void NewConnection() override {
    std::cerr << __func__ << std::endl;
  }
  
  void Reset() override {
    std::cerr << __func__ << std::endl;
  }
};



class TcpStateMachine;
template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, TcpStateMachine &m);

class RulesOnState;
class TcpStateMachine {
 public:
  template <class CharT, class Traits>
  friend std::basic_ostream<CharT, Traits>
  &operator<<(std::basic_ostream<CharT, Traits> &, TcpStateMachine &);
  
  TcpStateMachine();
  
  std::function<void(TcpInternalInterface *)> OnReceivePackage(
      const TcpHeader *header, uint16_t size);
  
  bool OnSequencePackage(TcpHeader &header, uint16_t size) {
    if (header.SequenceNumber() == host_last_ack_) {
      host_last_ack_ += size;
      return true;
    }
    return false;
  }
  
  void PrepareHeader(TcpHeader &header, uint16_t size) const {
    std::cerr << __func__ << std::endl;
    header.Window() = host_window_;
    
    if (header.Syn()) {
      header.SequenceNumber() = host_initial_seq_;
    } else if (header.Fin()) {
      header.SetAck(true);
      header.SequenceNumber() = host_next_seq_;
    } else if (header.Rst()) {
      assert(false);
    } else {
      header.SetAck(true);
    }
    
    if (header.Ack())
      header.AcknowledgementNumber() = host_last_ack_;
  }
  
  void PrepareResendHeader(TcpHeader &header, uint16_t) const {
    std::cerr << __func__;
    if (header.Ack())
      header.AcknowledgementNumber() = host_last_ack_;
    header.Window() = host_window_;
  }
  
  bool FinSent() {
    std::cerr << "Send Fin" << std::endl;
    if (state_ == State::kSynRcvd) {
      std::cerr << "kSynRcvd" << std::endl;
      state_ = State::kFinWait1;
      stage_ = Stage::kClosing;
    } else if (state_ == State::kEstab) {
      std::cerr << "kEstab" << std::endl;
      state_ = State::kFinWait1;
      stage_ = Stage::kClosing;
    } else if (state_ == State::kCloseWait) {
      std::cerr << "kCloseWait" << std::endl;
      state_ = State::kLastAck;
      stage_ = Stage::kClosing;
    } else {
      std::cerr << "Error" << std::endl;
      return false;
    }
    
    ++host_next_seq_;
    return true;
  }
  
  bool SynSent(uint32_t seq, uint16_t window) {
    if (state_ == State::kClosed) {
      host_initial_seq_ = seq;
      host_next_seq_ = seq+1;
      host_window_ = window;
      
      peer_last_ack_ = host_initial_seq_;
      peer_window_ = 1;
      
      state_ = State::kSynSent;
      stage_ = Stage::kHandShake;
      return true;
    }
    
    return false;
  }
  
  bool Listen() {
    if (state_ != State::kClosed)
      return false;
    SetState(0, State::kListen, Stage::kHandShake);
    return true;
  }
  
  Stage GetStage() const {
    return stage_;
  }
  
  State GetState() const {
    return state_;
  }
  
  operator Stage() const {
    return GetStage();
  }
  
  operator State() const {
    return GetState();
  }
  
  void SetState(int, State state, Stage stage) {
    state_ = state;
    stage_ = stage;
  }
  
  static Event ParseEvent(const TcpHeader *peer_header) {
    if (peer_header->Rst())
      return Event::kRstRecv;
    
    if (peer_header->Fin())
      return Event::kFinRecv;
    
    if (peer_header->Syn()) {
      if (peer_header->Ack())
        return Event::kSynAckRecv;
      else
        return Event::kSynRecv;
    }
    
    if (peer_header->Ack())
      return Event::kAckRecv;
    
    return Event::kNone;
  }
  
  std::pair<uint32_t, bool> GetSequenceNumber(uint16_t size) {
    if (host_next_seq_ + size > peer_last_ack_ + peer_window_)
      return {0, false};
    
    host_next_seq_ += size;
    return {host_next_seq_-size, true};
  }
  
  auto GetPeerLastAck() const {
    return peer_last_ack_;
  }
  
  auto GetPeerWindow() const {
    return peer_window_;
  }
  
  auto GetHostNextSeq() const {
    return host_next_seq_;
  }
  
  auto &GetHostWindow() {
    return host_window_;
  }
  const auto &GetHostWindow() const {
    return host_window_;
  }
  
  auto GetLastPackageSize() const {
    return last_package_size_;
  }
  
  auto GetInitialSequenceNumber() const {
    return host_initial_seq_;
  }
  
 private:
  bool CheckSeq() const {
    std::cerr << __func__ << header_->SequenceNumber() << " ";
    if (header_->SequenceNumber() <= peer_initial_seq_)
      return false;
    if (header_->SequenceNumber() + size_ >= host_last_ack_ + host_window_)
      return false;
    if (header_->SequenceNumber() < host_last_ack_)
      return false;
    return true;
  }
  
  bool CheckAck() const {
    std::cerr << __func__ << header_->AcknowledgementNumber() << std::endl;
    if (header_->AcknowledgementNumber() < peer_last_ack_)
      return false;
    if (header_->AcknowledgementNumber() > host_next_seq_)
      return false;
    if (header_->AcknowledgementNumber() < host_initial_seq_)
      return false;
    return true;
  }
  
  bool FullCheck() const {
    return CheckSeq() + CheckAck();
  }
  
  bool FinAckCheck() const {
    auto ret = FullCheck();
    if (header_->AcknowledgementNumber() != host_next_seq_+1)
      ret = false; // not Fin Ack
    return ret;
  }
  
  bool EmptyCheck() const {
    return true;
  }
  
  void FullUpdate() {
    peer_last_ack_ = std::max(header_->AcknowledgementNumber(),
        peer_last_ack_);
    peer_window_ = header_->Window();
  }
  
  void InitPeerInitialSeq(uint32_t seq) {
    peer_initial_seq_ = seq;
    host_last_ack_ = seq + 1;
  }
  
  void UpdateEmpty() {
    
  }

  void UpdateClosed2SynRcvd() {
    InitPeerInitialSeq(header_->SequenceNumber());
    peer_window_ = header_->Window();
    peer_last_ack_ = host_initial_seq_;
    
    state_ = State::kSynRcvd;
    stage_ = Stage::kHandShake;
  }
  
  void UpdateClosed2SynSent() {
    state_ = State::kSynSent;
    stage_ = Stage::kHandShake;
  }
  
  void UpdateSynRcvd2Estab() {
    FullUpdate();
    
    state_ = State::kEstab;
    stage_ = Stage::kEstab;
  }
  
  void UpdateSynRcvd2FinWait1() {
    state_ = State::kFinWait1;
    stage_ = Stage::kClosing;
  }
  
  void UpdateSynSent2SynRcvd() {
    UpdateClosed2SynRcvd();
    
    state_ = State::kSynRcvd;
    stage_ = Stage::kHandShake;
  }
  
  void UpdateSynSent2Estab() {
    peer_initial_seq_ = header_->SequenceNumber();
    peer_window_ = header_->Window();
    peer_last_ack_ = header_->AcknowledgementNumber();
    
    host_last_ack_ = peer_initial_seq_+1;
    
    state_ = State::kEstab;
    stage_ = Stage::kEstab;
  }
  
  void UpdateEstab2Estab() {
    FullUpdate();
  }
  
  void UpdateEstab2CloseWait() {
    FullUpdate();
    host_last_ack_ = header_->SequenceNumber() + 1;
    
    state_ = State::kCloseWait;
    stage_ = Stage::kClosing;
  }
  
  void UpdateEstab2FinWait1() {
    state_ = State::kFinWait1;
    stage_ = Stage::kClosing;
  }
  
  void UpdateFinWait12FinWait2() {
    FullUpdate();
    
    state_ = State::kFinWait2;
    stage_ = Stage::kClosing;
  }
  
  void UpdateFinWait12Closing() {
    UpdateEstab2CloseWait();
    
    state_ = State::kClosing;
    stage_ = Stage::kClosing;
  }
  
  void UpdateCloseWait2LastAck() {
    state_ = State::kCloseWait;
    stage_ = Stage::kClosing;
  }
  
  void UpdateFinWait22Closed() {
    UpdateEstab2CloseWait();
    
    state_ = State::kClosed;
    stage_ = Stage::kClosed;
  }
  
  void UpdateClosingToClosed() {
    FullUpdate();
    
    state_ = State::kClosed;
    stage_ = Stage::kClosed;
  }
  
  void UpdateLastAck2Closed() {
    FullUpdate();
    
    state_ = State::kClosed;
    stage_ = Stage::kClosed;
  }
  
  const TcpHeader *header_ = nullptr;
  uint16_t size_ = 0;
  
  State state_ = State::kClosed;
  Stage stage_ = Stage::kClosed;
  
  uint32_t peer_initial_seq_ = 0;
  uint32_t peer_last_ack_ = 0;
  uint32_t peer_window_ = 0;
  
  uint32_t host_initial_seq_ = 100;
  uint32_t host_next_seq_ = host_initial_seq_+1;
  uint32_t host_last_ack_ = 0;
  uint16_t host_window_ = 4096;
  
  uint16_t last_package_size_ = 0;

  static std::vector<std::pair<State, RulesOnState> > transition_rules_;
};

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, TcpStateMachine &m) {
  o << "------------" << std::endl;
  o << "State-" << m.GetState() << " Stage-" << m.GetStage() << std::endl;
  
  o << m.peer_initial_seq_ << " "
    << m.peer_last_ack_ << " "
    << m.peer_window_ << " "
    << std::endl
    
    << m.host_initial_seq_ << " "
    << m.host_next_seq_ << " "
    << m.host_last_ack_ << " "
    << m.host_window_ << " "
    << std::endl;
  
  o << "------------" << std::endl;

  return o;
}

} // namespace tcp_simulator

#endif // _TCP_STATE_MACHINE_H_
