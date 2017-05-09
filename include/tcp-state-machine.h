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
  
  kFinFinAckRecv, // Fin & FinAck
  
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
    "kFinFinAckRecv",
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
  
  TcpPacket RstHeader(const TcpPacket &source) {
    TcpPacket packet(nullptr, nullptr);
    const auto &source_header = source.GetHeader();
    auto &header = packet.GetHeader();
    
    header.SourcePort() = source_header.DestinationPort();
    header.DestinationPort() = source_header.SourcePort();
    
    header.SequenceNumber() = source_header.AcknowledgementNumber() + 1;
    header.SetRst(true);
    
    header.Checksum() = 0;
    header.Checksum() = packet.CalculateChecksum();
    return packet;
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
  
  virtual void NotifyOnConnected() = 0;
  virtual void NotifyOnReceivingMessage() = 0;
  virtual void NotifyOnNewConnection(size_t) = 0;
  
  virtual ~TcpInternalInterface() = default;
};

class TcpStateMachine;
template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, TcpStateMachine &m);

class RulesOnState;
class TransitionRule;

class TcpStateMachine {
 public:
  friend class TransitionRule;
  template <class CharT, class Traits>
  friend std::basic_ostream<CharT, Traits>
  &operator<<(std::basic_ostream<CharT, Traits> &, TcpStateMachine &);
  
  TcpStateMachine();
  
  // Update state on receving packet
  std::function<void(TcpInternalInterface *)> OnReceivePacket(
      const TcpHeader *header, uint16_t size);
  
  // Update state on sending packet
  bool FinSent();
  bool SynSent(uint32_t seq, uint16_t window);

  // Update state on time out
  void TimeOut() {}
  
  // Update state on listenning
  bool Listen() {
    if (state_ != State::kClosed)
      return false;
    SetState(0, State::kListen, Stage::kHandShake);
    return true;
  }
  
  // functions for preparing packet's header
  void PrepareHeader(TcpHeader &header, uint16_t size);
  
  void PrepareResendHeader(TcpHeader &header, uint16_t) const {
    std::cerr << __func__ << std::endl;
    if (header.Ack())
      header.AcknowledgementNumber() = host_last_ack_;
    header.Window() = host_window_;
  }
  
  std::pair<uint32_t, bool> NextSequenceNumber(uint16_t size) {
    if (host_next_seq_ + size > peer_last_ack_ + peer_window_)
      return {0, false};
    
    host_next_seq_ += size;
    std::cerr << "Seq gotten:" << host_next_seq_-size << std::endl;
    return {host_next_seq_-size, true};
  }
  
  // sequence out of order packets
  bool OnSequencePacket(TcpHeader &header, uint16_t size) {
    std::cerr << "SequenceNumber:" << header.SequenceNumber() << " "
              << "HostLastAck:" << host_last_ack_ << std::endl;
    if (header.SequenceNumber() == host_last_ack_) {
      host_last_ack_ += size;
      return true;
    }
    return false;
  }
  
  // member access
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
  
  auto GetLastPacketSize() const {
    return size_;
  }
  
  auto GetInitialSequenceNumber() const {
    return host_initial_seq_;
  }
  
 private:
  
  Event ParseEvent(const TcpHeader *peer_header);
  
  // header seq/ack check
  bool CheckSeq() const {
    std::cerr << __func__ << header_->SequenceNumber() << " ";
    if (header_->SequenceNumber() <= peer_initial_seq_)
      return false;
    if (header_->SequenceNumber() + size_ >= host_last_ack_ + host_window_)
      return false;
    if (header_->SequenceNumber() < host_last_ack_)
      return false;
    std::cerr << "Passed" << std::endl;
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
    std::cerr << "Passed" << std::endl;
    return true;
  }
  
  bool FullCheck() const {
    return CheckSeq() && CheckAck();
  }
  
  bool FinAckCheck() const {
    auto ret = FullCheck();
    if (header_->AcknowledgementNumber() != host_next_seq_)
      ret = false; // not Fin Ack
    return ret;
  }
  
  bool EmptyCheck() const {
    return true;
  }
  
  // functions for updating state
  void FullUpdate() {
    peer_last_ack_ = std::max(header_->AcknowledgementNumber(),
        peer_last_ack_);
    peer_window_ = header_->Window();
  }
  
  void InitPeerInitialSeq(uint32_t seq) {
    peer_initial_seq_ = seq;
    host_last_ack_ = seq + 1;
  }
  
  void UpdateEmpty() {}

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
  
  void UpdateFinWait12Closed() {
    FullUpdate();
    
    state_ = State::kClosed;
    stage_ = Stage::kClosed;
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
  
  // header in processing
  const TcpHeader *header_ = nullptr;
  uint16_t size_ = 0;
  
  // State
  State state_ = State::kClosed;
  Stage stage_ = Stage::kClosed;
  
  uint32_t peer_initial_seq_ = 0;
  uint32_t peer_last_ack_ = 0;
  uint32_t peer_window_ = 0;
  
  uint32_t host_initial_seq_ = 100;
  uint32_t host_next_seq_ = host_initial_seq_ + 1;
  uint32_t host_last_ack_ = 0;
  uint16_t host_window_ = 4096;
};

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, TcpStateMachine &m) {
  return
  o << "------------" << std::endl
    << "State-" << m.GetState() << " Stage-" << m.GetStage() << std::endl
  
    << m.peer_initial_seq_ << " "
    << m.peer_last_ack_ << " "
    << m.peer_window_ << " "
    << std::endl
    
    << m.host_initial_seq_ << " "
    << m.host_next_seq_ << " "
    << m.host_last_ack_ << " "
    << m.host_window_ << " "
    << std::endl
  
    << "------------" << std::endl;
}

} // namespace tcp_simulator

#endif // _TCP_STATE_MACHINE_H_
