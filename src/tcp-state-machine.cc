#include "tcp-state-machine.h"

namespace tcp_simulator {

std::vector<std::pair<State, RulesOnState> > TcpStateMachine::transition_rules_;

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
        
  std::pair<ReactionType, bool> CheckAndGetReaction(TcpStateMachine *machine) {
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
  
  auto GetReactionOnEvent(Event event, TcpStateMachine *machine) {
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

TcpStateMachine::TcpStateMachine() {
  
  if (transition_rules_.empty()) {
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
    /*
        std::cerr << "Rst Check" << std::endl;
        if (!CheckSeq()) {
          state_ = State::kClosed;
          stage_ = Stage::kClosed;
          return {[](auto &internal){internal.Reset();}, true};
        }
    */

    transition_rules_.emplace_back(State::kClosed, default_rule);
    transition_rules_.back().second
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
    /*
        // State::kClosed
        std::make_tuple(Ev::kSynRecv, St::kSynRcvd, Sg::kHandShake,
            empty_check,
            [](TcpStateMachine &machine, const TcpHeader *header, uint16_t) {
              machine.InitPeerInitialSeq(header->SequenceNumber());
              machine.peer_window_ = header->Window();
              machine.peer_last_ack_ = machine.host_initial_seq_;
            },
            send_synack, send_rst),
        std::make_tuple(Ev::kSynSent, St::kSynSent, Sg::kHandShake,
            empty_check, empty_update, send_syn, nope)}; 
    */
    
    transition_rules_.emplace_back(State::kListen, default_rule);
    transition_rules_.back().second
        .AddRule(Event::kSynRecv,
                 RuleOnEvent(&M::EmptyCheck,
                             &M::UpdateEmpty,
                             &M::UpdateEmpty,
                             [](auto internal){internal->NewConnection();},
                             nope));
    /*
        tranform_rule_[1] = {};  // State::kListen
        if (event == Event::kSynRecv)
          return {[](auto &internal){internal.NewConnection();}, true};
        return {[](auto &){}, true};
    */
    
    transition_rules_.emplace_back(State::kSynRcvd, default_rule);
    transition_rules_.back().second
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
    /*
        // State::kSynRcvd
        std::make_tuple(Ev::kAckRecv, St::kEstab, Sg::kEstab,
            full_check, full_update, none, send_rst),
        std::make_tuple(Ev::kFinSent, St::kFinWait1, Sg::kClosing,
            empty_check, empty_update, send_fin, nope)}; 
    */
    
    transition_rules_.emplace_back(State::kSynSent, default_rule);
    transition_rules_.back().second
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
    /*
        // State::kSynSent
        std::make_tuple(Ev::kSynRecv, St::kSynRcvd, Sg::kHandShake,
            empty_check,
            [](TcpStateMachine &machine, const TcpHeader *header, uint16_t) {
              machine.InitPeerInitialSeq(header->SequenceNumber());
              machine.peer_window_ = header->Window();
              machine.peer_last_ack_ = machine.host_initial_seq_;
            },
            send_ack, send_rst),
        std::make_tuple(Ev::kSynAckRecv, St::kEstab, Sg::kEstab,
            check_ack,
            [](TcpStateMachine &machine, const TcpHeader *header, uint16_t) {
              machine.peer_initial_seq_ = header->SequenceNumber();
              machine.peer_window_ = header->Window();
              machine.peer_last_ack_ = header->AcknowledgementNumber();
              
              machine.host_last_ack_ = machine.peer_initial_seq_+1;
            },
            send_ack, send_rst)};
    */
        
    transition_rules_.emplace_back(State::kEstab, default_rule);
    transition_rules_.back().second
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
    /*
    tranform_rule_[4] = { // State::kEstab
        std::make_tuple(Ev::kAckRecv, St::kEstab, Sg::kEstab,
            full_check, full_update, send_cond_ack, discard),
        std::make_tuple(Ev::kFinRecv, St::kCloseWait, Sg::kClosing,
            full_check, fin_update, response_fin, discard),
        std::make_tuple(Ev::kFinSent, St::kFinWait1, Sg::kClosing,
            empty_check, empty_update, send_fin, nope)};
    */
        
    transition_rules_.emplace_back(State::kFinWait1, default_rule);
    transition_rules_.back().second
        .AddRule(Event::kAckRecv,
                 RuleOnEvent(&M::FinAckCheck,
                             &M::UpdateFinWait12FinWait2,
                             &M::UpdateEmpty,
                             discard, discard))
        .AddRule(Event::kFinRecv,
                 RuleOnEvent(&M::CheckAck,
                             &M::UpdateFinWait12Closing,
                             &M::UpdateEmpty,
                             response_fin, discard))
        .AddRule(Event::kRstRecv, rule_reset);
    /*
    tranform_rule_[5] = { // State::kFinWait1
        std::make_tuple(Ev::kAckRecv, St::kFinWait2, Sg::kClosing, // FinAck
            finack_check, full_update, send_cond_ack, discard),
        std::make_tuple(Ev::kFinRecv, St::kClosing, Sg::kClosing,
            full_check, fin_update, response_fin, discard)};
    */
    
    transition_rules_.emplace_back(State::kCloseWait, default_rule);
    transition_rules_.back().second
        .AddRule(Event::kFinSent,
                 RuleOnEvent(&M::EmptyCheck,
                             &M::UpdateCloseWait2LastAck,
                             &M::UpdateEmpty,
                             none, nope))
        .AddRule(Event::kRstRecv, rule_reset);
    /*
    tranform_rule_[6] = { // State::kCloseWait
        std::make_tuple(Ev::kFinSent, St::kLastAck, Sg::kClosing,
            empty_check, empty_update, none, nope)};
    */
    
    transition_rules_.emplace_back(State::kFinWait2, default_rule);
    transition_rules_.back().second
        .AddRule(Event::kAckRecv,
                 RuleOnEvent(&M::EmptyCheck,
                             &M::UpdateEmpty,
                             &M::UpdateEmpty,
                             discard, discard))
        .AddRule(Event::kFinRecv,
                 RuleOnEvent(&M::FullCheck,
                             &M::UpdateFinWait22Closed,
                             &M::UpdateEmpty,
                             close, discard))
        .AddRule(Event::kRstRecv, rule_reset);
    /*
    tranform_rule_[7] = { // State::kFinWait2
        // skip kTimeWait
        std::make_tuple(Ev::kAckRecv, St::kFinWait2, Sg::kClosing,
            full_check, full_update, send_cond_ack, discard),
        std::make_tuple(Ev::kFinRecv, St::kClosed, Sg::kClosed,
            full_check, fin_update, close, discard)};
    */
    
    transition_rules_.emplace_back(State::kClosing, default_rule);
    transition_rules_.back().second
        .AddRule(Event::kAckRecv,
                 RuleOnEvent(&M::FinAckCheck,
                             &M::UpdateClosingToClosed,
                             &M::UpdateEmpty,
                             discard, discard))
        .AddRule(Event::kRstRecv, rule_reset);
    /*
    tranform_rule_[8] = { // State::kClosing
        std::make_tuple(Ev::kAckRecv, St::kClosed, Sg::kClosed, // FinAck
            finack_check, full_update, discard, discard), };
    */
        
    transition_rules_.emplace_back(State::kLastAck, default_rule);
    transition_rules_.back().second
        .AddRule(Event::kAckRecv,
                 RuleOnEvent(&M::FinAckCheck,
                             &M::UpdateLastAck2Closed,
                             &M::UpdateEmpty,
                             discard, discard))
        .AddRule(Event::kRstRecv, rule_reset);
    /*
    tranform_rule_[9] = { // State::kLastAck
        std::make_tuple(Ev::kAckRecv, St::kClosed, Sg::kClosed, // FinAck
            finack_check, full_update, discard, discard)};
    */
    
    transition_rules_.emplace_back(State::kTimeWait, default_rule);
    /*
    tranform_rule_[10] = {}; // State::kTimeWait
    */

    /*
    tranform_rule_[11] = { // default
        std::make_tuple(Ev::kNone, St::kClosed, Sg::kClosed,
            empty_check, empty_check, nope, discard)};
    */
  }
}

std::function<void(TcpInternalInterface *)> TcpStateMachine::OnReceivePackage(
    const TcpHeader *header, uint16_t size) {
  assert(header);
  header_ = header;
  size_ = size;
  
  auto event = ParseEvent(header);
  
  auto ite = std::find_if(transition_rules_.begin(), transition_rules_.end(),
      [this](auto &x){
        return x.first == state_;
      });
  
  if (ite != transition_rules_.end())
    return ite->second.GetReactionOnEvent(event, this).first;
  throw std::runtime_error("TcpStateMachine critical Error.");
}

} // namespace tcp_simulator
