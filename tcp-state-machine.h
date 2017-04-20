#ifndef _TCP_STATE_MACHINE_H_
#define _TCP_STATE_MACHINE_H_

#include <ostream>
#include <tuple>
#include <utility>
#include <vector>

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
  kFinSent,
  kFinAckRecv,
  
  kRst
};

enum class Action {
  kNone = 0,
  kSendSyn,
  kSendAck,
  kSendSynAck,
  kClose,
  
  kWaitAck,
  kWaitFin
};

class TcpStateMachine {
 public:
  TcpStateMachine() {
    using St = State;
    using Ac = Action;
    using Sg = Stage;
    using Ev = Event;
    if (tranform_rule_.size() == 0) {
      tranform_rule_.resize(static_cast<int>(St::kTimeWait)+1);
      tranform_rule_[0] = {}; // State::kClosed

      tranform_rule_[1] = {  // State::kListen
          std::make_tuple(Ev::kSynRecv, St::kSynRcvd, Ac::kSendSynAck,
                          Sg::kHandShake)};  
          
      tranform_rule_[2] = {  // State::kSynRcvd
          std::make_tuple(Ev::kAckRecv, St::kEstab, Ac::kNone,
                          Sg::kEstab),
          std::make_tuple(Ev::kFinSent, St::kFinWait1, Ac::kWaitAck,
                          Sg::kClosing)}; 
          
      tranform_rule_[3] = { // State::kSynSent
          std::make_tuple(Ev::kSynRecv, St::kSynRcvd, Ac::kSendAck,
                          Sg::kHandShake),
          std::make_tuple(Ev::kSynAckRecv, St::kEstab, Ac::kNone,
                          Sg::kEstab)}; 
          
      tranform_rule_[4] = { // State::kEstab
          std::make_tuple(Ev::kFinRecv, St::kCloseWait, Ac::kSendAck,
                          Sg::kClosing),
          std::make_tuple(Ev::kFinSent, St::kFinWait1, Ac::kWaitAck, // or Fin
                          Sg::kClosing)};
      
      tranform_rule_[5] = { // State::kFinWait1
          std::make_tuple(Ev::kFinAckRecv, St::kFinWait2, Ac::kWaitFin,
                          Sg::kClosing),
          std::make_tuple(Ev::kFinRecv, St::kClosing, Ac::kSendAck,
                          Sg::kClosing)};
      
      tranform_rule_[6] = { // State::kCloseWait
          std::make_tuple(Ev::kFinSent, St::kLastAck, Ac::kWaitAck,
                          Sg::kClosing)};
      
      tranform_rule_[7] = { // State::FinWait2
          // skip kTimeWait
          // can't signal TcpManager to close socket!
          std::make_tuple(Ev::kFinRecv, St::kClosed, Ac::kSendAck,
                          Sg::kClosed)}; 
      
      tranform_rule_[8] = { // State::kClosing
          std::make_tuple(Ev::kFinAckRecv, St::kClosed, Ac::kClose,
                          Sg::kClosed)};
          
      tranform_rule_[9] = { // State::kLastAck
          std::make_tuple(Ev::kFinAckRecv, St::kClosed, Ac::kClose,
                          Sg::kClosed)};
      
      tranform_rule_[10] = {}; // State::kTimeWait
    }
  }
  
  void Init(State init_state) {
    if (init_state != State::kListen && init_state != State::kSynSent)
      return ;
    state_ = init_state;
  }
  
  Action GetReaction(Event event) {
    const auto &rule = tranform_rule_[static_cast<int>(state_)];
    for (auto condition : rule) {
      if (std::get<0>(condition) == event) {
        return std::get<2>(condition);
      }
    }
    
    return Action::kNone;
  }
  
  Action OnEvent(Event event) {
    if (SpecialRules(event))
      return Action::kNone;
    
    const auto &rule = tranform_rule_[static_cast<int>(state_)];
    for (auto condition : rule) {
      if (std::get<0>(condition) == event) {
        state_ = std::get<1>(condition);
        stage_ = std::get<3>(condition);
        return std::get<2>(condition);
      }
    }
    
    return Action::kNone;
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
  
 private:
  
  int SpecialRules(Event event) {
    if (event == Event::kRst) {
      state_ = State::kClosed;
      stage_ = Stage::kClosed;
      return 1;
    }
    
    return 0;
  }
  
  State state_ = State::kClosed;
  Stage stage_ = Stage::kClosed;
  
  static std::vector<std::vector<std::tuple<Event, State, Action, Stage> > >
      tranform_rule_;
};

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, State state) {
  static const std::vector<std::string> state_string{
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
    "kFinSend",
    "kFinAckRecv"
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

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, Action action) {
  
  static const std::vector<std::string> action_string{
    "kNone",
    "kSendSyn",
    "kSendAck",
    "kSendSynAck",
    "kClose",
    "kWaitAck"
  };
  
  return o << action_string.at(static_cast<int>(action));
}

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &o, TcpStateMachine &m) {
  return o << "State-" << m.GetState() << " Stage-" << m.GetStage();
}


#endif // _TCP_STATE_MACHINE_H_