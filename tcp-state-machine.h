#ifndef _TCP_STATE_MACHINE_H_
#define _TCP_STATE_MACHINE_H_

#include <cassert>

#include <functional>
#include <iostream>
#include <tuple>
#include <utility>
#include <vector>

#include "tcp-buffer.h"

using XXX = TcpHeader;

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
  
  kRst
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
    "kRst"
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

class TcpInternalInterface {
 public:
  virtual void SendAck() = 0;
  virtual void SendConditionAck() = 0;
  virtual void SendSyn(uint32_t, uint16_t) = 0;
  virtual void SendSynAck(uint32_t, uint16_t) = 0;
  virtual void SendFin() = 0;
  virtual void SendRst() = 0;
  virtual void Accept() = 0;
  virtual void Discard() = 0;
  virtual void Close() = 0;
  virtual void NewConnection() = 0;
  
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
  
  void SendSyn(uint32_t, uint16_t) override {
    std::cerr << __func__ << std::endl;
  }
  void SendSynAck(uint32_t, uint16_t) override {
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
};

class TcpStateMachine;
template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, TcpStateMachine &m);

class TcpStateMachine {
 public:
  template <class CharT, class Traits>
  friend std::basic_ostream<CharT, Traits>
  &operator<<(std::basic_ostream<CharT, Traits> &, TcpStateMachine &);
  
  TcpStateMachine(TcpInternalInterface *internal) {
    using St = State;
    using Sg = Stage;
    using Ev = Event;
    internal_ = internal;
    
    if (tranform_rule_.size() == 0) {
    //  auto check_seq = [](TcpStateMachine &machine, const TcpHeader *header,
    //                      uint16_t size) {
    //        return machine.CheckPeerSeq(*header, size);
    //      };
      auto check_ack = [](TcpStateMachine &machine,const TcpHeader *header,
                          uint16_t size) {
            return machine.CheckPeerAck(*header, size);
          };
      auto full_check = [](TcpStateMachine &machine,const TcpHeader *header,
                           uint16_t size) {
            return machine.CheckPeerSeq(*header, size) +
                   machine.CheckPeerAck(*header, size)*10;
          };
      auto finack_check = [full_check](TcpStateMachine &machine,
                                       const TcpHeader *header,
                                       uint16_t size) {
            auto ret = full_check(machine, header, size);
            if (ret == 0 &&
                header->AcknowledgementNumber() != machine.host_next_seq_+1)
              ret = 1; // not Fin Ack
            return ret;
          };
      auto full_update = [](TcpStateMachine &machine, const TcpHeader *header,
                            uint16_t size) {
            machine.FullUpdate(*header, size);
          };
      auto empty_check = [](TcpStateMachine &, const TcpHeader *, uint16_t) {
            return 0;
          };
      auto empty_update = [](TcpStateMachine &, const TcpHeader *, uint16_t) {};
      
      auto send_ack = [](TcpStateMachine &machine){
            machine.internal_->Accept();
            machine.internal_->SendAck();
          };
      auto send_cond_ack = [](TcpStateMachine &machine){
            machine.internal_->Accept();
            machine.internal_->SendConditionAck();
          };
      auto send_syn = [](TcpStateMachine &machine){
            machine.internal_->Accept();
            machine.internal_->SendSyn(machine.host_initial_seq_,
                                       machine.host_window_);
          };
      auto send_synack = [](TcpStateMachine &machine){
            machine.internal_->Accept();
            machine.internal_->SendSynAck(machine.host_initial_seq_,
                                          machine.host_window_);
          };
      auto send_fin = [](TcpStateMachine &machine){
            machine.internal_->Accept();
            machine.internal_->SendFin();
          };
    //  auto send_rst = [](TcpStateMachine &machine){
    //        machine.internal_->Accept();
    //        machine.internal_->SendRst();
    //      };
    //  auto accept = [](TcpStateMachine &machine){
    //        machine.internal_->Accept();
    //      };
      auto close = [](TcpStateMachine &machine){
            machine.internal_->Close();
          };
      auto discard = [](TcpStateMachine &machine){
            machine.internal_->Discard();
          };
      auto nope = [](TcpStateMachine &machine){
            throw std::runtime_error("TcpState unexpected action.");
          };
      auto none = [](TcpStateMachine &machine){};
      
      tranform_rule_.resize(static_cast<int>(St::kTimeWait)+2);
      tranform_rule_[0] = { // State::kClosed
          std::make_tuple(Ev::kSynRecv, St::kSynRcvd, Sg::kHandShake,
              empty_check,
              [](TcpStateMachine &machine, const TcpHeader *head, uint16_t) {
                machine.peer_initial_seq_ = head->SequenceNumber();
              },
              send_synack, discard),
          std::make_tuple(Ev::kSynSent, St::kSynSent, Sg::kHandShake,
              empty_check, empty_update, send_syn, nope)}; 

      tranform_rule_[1] = {};  // State::kListen
            
          
      tranform_rule_[2] = {  // State::kSynRcvd
          std::make_tuple(Ev::kAckRecv, St::kEstab, Sg::kEstab,
              full_check, full_update, none, discard),
          std::make_tuple(Ev::kFinSent, St::kFinWait1, Sg::kClosing,
              empty_check, empty_update, send_fin, nope)}; 
          
      tranform_rule_[3] = { // State::kSynSent
          std::make_tuple(Ev::kSynRecv, St::kSynRcvd, Sg::kHandShake,
              empty_check, 
              [](TcpStateMachine &machine, const TcpHeader *header, uint16_t) {
                machine.peer_initial_seq_ = header->SequenceNumber();
                machine.peer_window_ = header->Window();
              },
              send_ack, discard),
          std::make_tuple(Ev::kSynAckRecv, St::kEstab, Sg::kEstab,
              check_ack,
              [](TcpStateMachine &machine, const TcpHeader *header, uint16_t) {
                machine.peer_initial_seq_ = header->SequenceNumber();
                machine.peer_window_ = header->Window();
                machine.peer_last_ack_ = header->AcknowledgementNumber();
              },
              send_ack, discard)};
          
      tranform_rule_[4] = { // State::kEstab
          std::make_tuple(Ev::kAckRecv, St::kEstab, Sg::kEstab,
              full_check, full_update, send_cond_ack, discard),
          std::make_tuple(Ev::kFinRecv, St::kCloseWait, Sg::kClosing,
              full_check, full_update, send_ack, discard),
          std::make_tuple(Ev::kFinSent, St::kFinWait1, Sg::kClosing,
              empty_check, empty_update, send_fin, nope)};
      
      tranform_rule_[5] = { // State::kFinWait1
          std::make_tuple(Ev::kAckRecv, St::kFinWait2, Sg::kClosing, // FinAck
              finack_check, full_update, send_cond_ack, discard),
          std::make_tuple(Ev::kFinRecv, St::kClosing, Sg::kClosing,
              full_check, full_update, send_ack, discard)};
      
      tranform_rule_[6] = { // State::kCloseWait
          std::make_tuple(Ev::kFinSent, St::kLastAck, Sg::kClosing,
              empty_check, empty_update, none, nope)};
      
      tranform_rule_[7] = { // State::kFinWait2
          // skip kTimeWait
          std::make_tuple(Ev::kAckRecv, St::kFinWait2, Sg::kClosing,
              full_check, full_update, send_cond_ack, discard),
          std::make_tuple(Ev::kFinRecv, St::kClosed, Sg::kClosed,
              full_check, full_update, close, discard)};
      
      tranform_rule_[8] = { // State::kClosing
          std::make_tuple(Ev::kAckRecv, St::kClosed, Sg::kClosed, // FinAck
              finack_check, full_update, discard, discard), };
          
      tranform_rule_[9] = { // State::kLastAck
          std::make_tuple(Ev::kAckRecv, St::kClosed, Sg::kClosed, // FinAck
              finack_check, full_update, discard, discard)};
      
      tranform_rule_[10] = {}; // State::kTimeWait
      
      tranform_rule_[11] = { // default
          std::make_tuple(Ev::kNone, St::kClosed, Sg::kClosed,
              empty_check, empty_check, nope, discard)};
    }
  }
  
  void OnReceivePackage(const TcpHeader *header, uint16_t size) {
    assert(header);
    
    auto event = ParseEvent(header);
    std::cerr << __func__ << ": " << event << " ";
    constexpr int Check = 3;
    constexpr int Update = 4;
    constexpr int Success = 5;
    constexpr int Failed = 6;
    
    last_package_size_ = size;
    
    if (SpecialRule(event, header, size)) { // Event::kRst State::Listen
      std::cerr << "SpecialRule!" << std::endl;
      return ;
    } 
    
    const auto &rule = tranform_rule_[static_cast<int>(state_)];
    
    for (auto &condition : rule) {
      if (std::get<0>(condition) == event) {
        auto check_result = std::get<Check>(condition)(*this, header, size);
        std::cerr << "!" << check_result << " ";
        if (check_result == 0) {
          state_ = std::get<1>(condition);
          stage_ = std::get<2>(condition);

          std::get<Update>(condition)(*this, header, size);
          std::get<Success>(condition)(*this);
        } else if (check_result > 0) {

          std::get<Update>(condition)(*this, header, size);
          std::get<Success>(condition)(*this);
        } else {
          std::get<Failed>(condition)(*this);
        }
        
        std::cout << " end" << std::endl;
        return ;
      }
    }
    
    std::get<Failed>(tranform_rule_.back().front())(*this); // default
    std::cerr << " end" << std::endl;
  }
  
  bool OnSequencePackage(TcpHeader &header, uint16_t size) {
    if (header.SequenceNumber() == host_last_ack_) {
      host_last_ack_ += size;
      return true;
    }
    return false;
  }
  
  void PrepareDataHeader(TcpHeader &header, uint16_t size) const {
    header.SetAck(true);
    header.AcknowledgementNumber() = host_last_ack_;
    header.Window() = host_window_;
  }
  
  void PrepareSpecialHeader(TcpHeader &header) const {
    if (header.Ack())
      header.AcknowledgementNumber() = host_last_ack_;
    header.SequenceNumber() = host_next_seq_;
    header.Window() = host_window_;
  }
  
  void PrepareResendHeader(TcpHeader &header) const {
    if (header.Ack())
      header.AcknowledgementNumber() = host_last_ack_;
    header.Window() = host_window_;
  }
  
  bool SpecialRule(Event event, const TcpHeader *header, uint16_t size) {
    if (event == Event::kRst) {
      if (!CheckPeerSeq(*header, size)) {
        state_ = State::kClosed;
        stage_ = Stage::kClosed;
        internal_->Close();  
      }
      return true;
    } else if (state_ == State::kListen) {
      if (event == Event::kSynRecv)
        internal_->NewConnection();
      return true;
    }
    return false;
  }
  
  bool SendFin() {
    if (state_ == State::kSynRcvd) {
      state_ = State::kFinWait1;
      stage_ = Stage::kClosing;
    } else if (state_ == State::kEstab) {
      state_ = State::kFinWait1;
      stage_ = Stage::kClosing;
    } else if (state_ == State::kCloseWait) {
      state_ = State::kLastAck;
      stage_ = Stage::kClosing;
    } else {
      return false;
    }
    
    internal_->SendFin();
    ++host_next_seq_;
    return true;
  }
  
  bool SendSyn(uint32_t seq, uint16_t window) {
    if (state_ == State::kClosed) {
      host_initial_seq_ = seq;
      host_next_seq_ = seq+1;
      host_window_ = window;
      
      state_ = State::kSynSent;
      stage_ = Stage::kHandShake;
      internal_->SendSyn(seq, window);
      return true;
    }
    
    return false;
  }
  
  bool Listen() {
    if (state_ != State::kClosed)
      return false;
    SetState(0, State::kListen, Stage::kHandShake);
  }
  
  Stage GetStage() const {
    return *this;
  }
  
  State GetState() const {
    return *this;
  }
  
  operator Stage() const {
    return stage_;
  }
  
  operator State() const {
    return state_;
  }
  
  void SetState(int, State state, Stage stage) {
    state_ = state;
    stage_ = stage;
  }
  
  static Event ParseEvent(const TcpHeader *peer_header) {
    if (peer_header->Rst())
      return Event::kRst;
    
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
  int CheckPeerSeq(const TcpHeader &peer_header, uint16_t size) {
    if (peer_header.SequenceNumber() <= peer_initial_seq_)
      return -1;
    if (peer_header.SequenceNumber() + size >= host_last_ack_ + host_window_)
      return -2;
    if (peer_header.SequenceNumber() < host_last_ack_)
      return -3;
    return 0;
  }
  
  int CheckPeerAck(const TcpHeader &peer_header, uint16_t size) {
    if (peer_header.AcknowledgementNumber() < peer_last_ack_)
      return -1;
    if (peer_header.AcknowledgementNumber() > host_next_seq_)
      return -2;
    if (peer_header.AcknowledgementNumber() < host_initial_seq_)
      return -3;
    return 0;
  }
  
  void FullUpdate(const TcpHeader &peer_header, uint16_t size) {
    peer_last_ack_ = std::max(peer_header.AcknowledgementNumber(),
        peer_last_ack_);
    peer_window_ = peer_header.Window();
  }
  
  State state_ = State::kClosed;
  Stage stage_ = Stage::kClosed;
  
  uint32_t peer_initial_seq_ = 0;
  uint32_t peer_last_ack_ = 0;
  uint32_t peer_window_ = 0;
  
  uint32_t host_initial_seq_ = 100;
  uint32_t host_next_seq_ = host_initial_seq_+1;
  uint32_t host_last_ack_ = 0;
  uint32_t host_window_ = 4096;
  
  uint16_t last_package_size_ = 0;
  
  TcpInternalInterface *internal_ = nullptr;
  
  static std::vector<std::vector<
      std::tuple<Event, State, Stage,
                 // < 0 discard, > 0 no discard, == 0 transstate
                 std::function<int(TcpStateMachine &, const TcpHeader *,
                                   uint16_t)>, // Check
                 std::function<void(TcpStateMachine &, const TcpHeader *,
                                   uint16_t)>, // Update
                 std::function<void(TcpStateMachine &)>, // action on success
                 std::function<void(TcpStateMachine &)> // action on failure
                > > > tranform_rule_;
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

#endif // _TCP_STATE_MACHINE_H_