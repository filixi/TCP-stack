#include "tcp-state-machine.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace tcp_simulator {

class RuleOnEvent {
 public:
  using CheckType = std::function<bool(const TcpStateMachine *)>;
  using UpdateType = std::function<void(TcpStateMachine *)>;
  using ReactionType = std::function<void(TcpInternalInterface *)>;
  RuleOnEvent(const CheckType &check,
              const UpdateType &update_on_success,
              const UpdateType &update_on_failure,
              const ReactionType &action_on_success,
              const ReactionType &action_on_failure)
      : state_checking_(check),
        update_on_success_(update_on_success),
        update_on_failure_(update_on_failure),
        action_on_success_(action_on_success),
        action_on_failure_(action_on_failure) {}
        
  std::pair<ReactionType, bool>
  CheckAndGetReaction(TcpStateMachine *machine) const {
    if (state_checking_(machine)) {
      update_on_success_(machine);
      return {action_on_success_, true};
    }
    update_on_failure_(machine);
    return {action_on_failure_, false};
  }
  
 private:
  CheckType state_checking_; // seq/ack
  UpdateType update_on_success_;
  UpdateType update_on_failure_;
  ReactionType action_on_success_;
  ReactionType action_on_failure_;
};

class RulesOnState {
 public:
  RulesOnState(const RuleOnEvent &default_rule)
      : default_rule_(default_rule) {}
  
  RulesOnState &AddRule(Event event, const RuleOnEvent &rule) {
    assert(std::none_of(rules_.begin(), rules_.end(),
        [this, event](auto &x){
          return x.first == event;
        }));
    
    rules_.emplace_back(event, rule);
    return *this;
  }
  
  auto GetReactionOnEvent(Event event, TcpStateMachine *machine) const {
    auto ite = std::find_if(rules_.begin(), rules_.end(),
        [event](auto &x){
          return x.first == event;
        });
    
    if (ite != rules_.end())
      return ite->second.CheckAndGetReaction(machine);
    return default_rule_.CheckAndGetReaction(machine);
  }
  
 private:
  std::vector<std::pair<Event, RuleOnEvent> > rules_;
  RuleOnEvent default_rule_;
};

class TransitionRule {
 public:
  using RulesType = std::vector<std::pair<State, RulesOnState>>;
  
  TransitionRule() = delete;
  
  static const RulesType &GetRule() {
    // DCLP with atomic :)
    if (transition_rule_ == nullptr) {
      std::lock_guard<std::mutex> guard(mtx_);
      if (transition_rule_ == nullptr) {
        std::atomic<RulesType *> ptr;
        ptr.store(new RulesType);
        InitRule(ptr.load());
        
        transition_rule_.reset(ptr.load());
      }
    }
    
    return *transition_rule_;
  }
  
 private:
  static void InitRule(RulesType *rules);
  
  static std::unique_ptr<std::vector<std::pair<State, RulesOnState> > >
      transition_rule_;
  static std::mutex mtx_;
};

std::unique_ptr<std::vector<std::pair<State, RulesOnState> > >
      TransitionRule::transition_rule_;
std::mutex TransitionRule::mtx_;

Event ParseEvent(const TcpHeader *peer_header) {
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

TcpStateMachine::TcpStateMachine() {}

std::function<void(TcpInternalInterface *)> TcpStateMachine::OnReceivePacket(
    const TcpHeader *header, uint16_t size) {
  assert(header);

  header_ = header;
  size_ = size;
  auto event = ParseEvent(header);
  
  auto &transition_rule = TransitionRule::GetRule();
  
  auto ite = std::find_if(transition_rule.begin(), transition_rule.end(),
      [this](auto &x){
        return x.first == state_;
      });
  
  if (ite != transition_rule.end())
    return ite->second.GetReactionOnEvent(event, this).first;
  throw std::runtime_error("TcpStateMachine critical Error.");
}

bool TcpStateMachine::FinSent() {
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

bool TcpStateMachine::SynSent(uint32_t seq, uint16_t window) {
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

void TcpStateMachine::PrepareHeader(TcpHeader &header, uint16_t size) const {
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

void TransitionRule::InitRule(RulesType *rules) {
  auto &transition_rules = *rules;
  // action
  auto send_ack = [](TcpInternalInterface *internal){
        internal->Accept();
        internal->SendAck();
      };
  auto send_cond_ack = [](TcpInternalInterface *internal){
        internal->Accept();
        internal->SendConditionAck();
      };
  auto send_syn = [](TcpInternalInterface *internal){
        internal->Accept();
        internal->SendSyn();
      };
  auto send_synack = [](TcpInternalInterface *internal){
        internal->Accept();
        internal->SendSynAck();
      };
  auto send_fin = [](TcpInternalInterface *internal){
        internal->Accept();
        internal->SendFin();
      };
  auto send_rst = [](TcpInternalInterface *internal){
        internal->Discard();
        internal->SendRst();
      };
//  unused
//  auto accept = [](TcpInternalInterface &internal){
//        internal->Accept();
//      };
  auto response_fin = [](TcpInternalInterface *internal) {
        internal->Accept();
        internal->SendAck();
      };
  auto close = [](TcpInternalInterface *internal) {
        internal->Close();
      };
  auto discard = [](TcpInternalInterface *internal) {
        internal->Discard();
      };
  auto nope = [](TcpInternalInterface *internal) {
        throw std::runtime_error("TcpState unexpected action error");
      };
  auto none = [](TcpInternalInterface *internal) {};
  
  using M = TcpStateMachine;
  
  RuleOnEvent default_rule(
      [](auto...) {return true;},
      [](auto...) {},
      [](auto...) {},
      [](auto...) {},
      [](auto...) {});
  
  RuleOnEvent rule_reset(
      &M::CheckSeq,
      [](auto machine) {
        machine->state_ = State::kClosed;
        machine->stage_ = Stage::kClosed;
      },
      &M::UpdateEmpty,
      [](auto internal) {
        internal->Reset();
      },
      nope);

  transition_rules.emplace_back(State::kClosed, default_rule);
  transition_rules.back().second
      .AddRule(Event::kSynRecv,
               RuleOnEvent(&M::EmptyCheck,
                           &M::UpdateClosed2SynRcvd,
                           &M::UpdateEmpty,
                           send_synack, send_rst))
      .AddRule(Event::kSynSent,
               RuleOnEvent(&M::EmptyCheck,
                           &M::UpdateClosed2SynSent,
                           &M::UpdateEmpty,
                           send_syn, nope));
  
  transition_rules.emplace_back(State::kListen, default_rule);
  transition_rules.back().second
      .AddRule(Event::kSynRecv,
               RuleOnEvent(&M::EmptyCheck,
                           &M::UpdateEmpty,
                           &M::UpdateEmpty,
                           [](auto internal){internal->NewConnection();},
                           nope));
  
  transition_rules.emplace_back(State::kSynRcvd, default_rule);
  transition_rules.back().second
      .AddRule(Event::kAckRecv,
               RuleOnEvent(&M::FullCheck,
                           &M::UpdateSynRcvd2Estab,
                           &M::UpdateEmpty,
                           none, send_rst))
      .AddRule(Event::kFinSent,
               RuleOnEvent(&M::EmptyCheck,
                           &M::UpdateSynRcvd2FinWait1,
                           &M::UpdateEmpty,
                           send_fin, nope))
      .AddRule(Event::kRstRecv, rule_reset);
      
  transition_rules.emplace_back(State::kSynSent, default_rule);
  transition_rules.back().second
      .AddRule(Event::kSynRecv,
               RuleOnEvent(&M::EmptyCheck,
                           &M::UpdateSynSent2SynRcvd,
                           &M::UpdateEmpty,
                           send_ack, send_rst))
      .AddRule(Event::kSynAckRecv,
               RuleOnEvent(&M::CheckAck,
                           &M::UpdateSynSent2Estab,
                           &M::UpdateEmpty,
                           send_ack, send_rst))
      .AddRule(Event::kRstRecv, rule_reset);

  transition_rules.emplace_back(State::kEstab, default_rule);
  transition_rules.back().second
      .AddRule(Event::kAckRecv,
               RuleOnEvent(&M::FullCheck,
                           &M::UpdateEstab2Estab,
                           &M::UpdateEmpty,
                           send_cond_ack, discard))
      .AddRule(Event::kFinRecv,
               RuleOnEvent(&M::FullCheck,
                           &M::UpdateEstab2CloseWait,
                           &M::UpdateEmpty,
                           response_fin, discard))
      .AddRule(Event::kFinSent,
               RuleOnEvent(&M::EmptyCheck,
                           &M::UpdateEstab2FinWait1,
                           &M::UpdateEmpty,
                           send_fin, nope))
      .AddRule(Event::kRstRecv, rule_reset);

  transition_rules.emplace_back(State::kFinWait1, default_rule);
  transition_rules.back().second
      .AddRule(Event::kAckRecv,
               RuleOnEvent(&M::FinAckCheck,
                           &M::UpdateFinWait12FinWait2,
                           &M::UpdateEmpty,
                           discard, discard))
      .AddRule(Event::kFinRecv,
               RuleOnEvent(&M::FullCheck,
                           &M::UpdateFinWait12Closing,
                           &M::UpdateEmpty,
                           response_fin, discard))
      .AddRule(Event::kRstRecv, rule_reset);

  transition_rules.emplace_back(State::kCloseWait, default_rule);
  transition_rules.back().second
      .AddRule(Event::kFinSent,
               RuleOnEvent(&M::EmptyCheck,
                           &M::UpdateCloseWait2LastAck,
                           &M::UpdateEmpty,
                           none, nope))
      .AddRule(Event::kRstRecv, rule_reset);

  transition_rules.emplace_back(State::kFinWait2, default_rule);
  transition_rules.back().second
      .AddRule(Event::kAckRecv,
               RuleOnEvent(&M::EmptyCheck,
                           &M::UpdateEmpty,
                           &M::UpdateEmpty,
                           discard, discard))
      .AddRule(Event::kFinRecv, // skip kTimeWait
               RuleOnEvent(&M::FullCheck,
                           &M::UpdateFinWait22Closed,
                           &M::UpdateEmpty,
                           close, discard))
      .AddRule(Event::kRstRecv, rule_reset);
  
  transition_rules.emplace_back(State::kClosing, default_rule);
  transition_rules.back().second
      .AddRule(Event::kAckRecv,
               RuleOnEvent(&M::FinAckCheck,
                           &M::UpdateClosingToClosed,
                           &M::UpdateEmpty,
                           discard, discard))
      .AddRule(Event::kRstRecv, rule_reset);
      
  transition_rules.emplace_back(State::kLastAck, default_rule);
  transition_rules.back().second
      .AddRule(Event::kAckRecv,
               RuleOnEvent(&M::FinAckCheck,
                           &M::UpdateLastAck2Closed,
                           &M::UpdateEmpty,
                           discard, discard))
      .AddRule(Event::kRstRecv, rule_reset);
  
  transition_rules.emplace_back(State::kTimeWait, default_rule);
}

} // namespace tcp_simulator