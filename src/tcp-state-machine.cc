#include "tcp-state-machine.h"

namespace tcp_simulator {

std::vector<std::vector<
    std::tuple<
        Event, State, Stage,
        // < 0 discard, > 0 no discard, == 0 transstate
        std::function<int(TcpStateMachine &, const TcpHeader *,
                         uint16_t)>, // Check
        std::function<void(TcpStateMachine &, const TcpHeader *,
                         uint16_t)>, // Update
        std::function<void(TcpInternalInterface &)>, // action on success
        std::function<void(TcpInternalInterface &)> // action on failure
    > > > TcpStateMachine::tranform_rule_;

TcpStateMachine::TcpStateMachine() {
  using St = State;
  using Sg = Stage;
  using Ev = Event;
  
  if (tranform_rule_.size() == 0) {
  //  unused
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
                 machine.CheckPeerAck(*header, size);
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
    auto fin_update = [](TcpStateMachine &machine, const TcpHeader *header,
                          uint16_t size) {
          machine.FullUpdate(*header, size);
          machine.host_last_ack_ = header->SequenceNumber() + 1;
        };
    auto empty_check = [](TcpStateMachine &, const TcpHeader *, uint16_t) {
          return 0;
        };
    auto empty_update = [](TcpStateMachine &, const TcpHeader *, uint16_t) {};
    
    auto send_ack = [](TcpInternalInterface &internal){
          internal.Accept();
          internal.SendAck();
        };
    auto send_cond_ack = [](TcpInternalInterface &internal){
          internal.Accept();
          internal.SendConditionAck();
        };
    auto send_syn = [](TcpInternalInterface &internal){
          internal.Accept();
          internal.SendSyn();
        };
    auto send_synack = [](TcpInternalInterface &internal){
          internal.Accept();
          internal.SendSynAck();
        };
    auto send_fin = [](TcpInternalInterface &internal){
          internal.Accept();
          internal.SendFin();
        };
    auto send_rst = [](TcpInternalInterface &internal){
          internal.Discard();
          internal.SendRst();
        };
  //  unused
  //  auto accept = [](TcpInternalInterface &internal){
  //        internal->Accept();
  //      };
    auto response_fin = [](TcpInternalInterface &internal) {
          internal.Accept();
          internal.SendAck();
        };
    auto close = [](TcpInternalInterface &internal) {
          internal.Close();
        };
    auto discard = [](TcpInternalInterface &internal) {
          internal.Discard();
        };
    auto nope = [](TcpInternalInterface &internal) {
          throw std::runtime_error("TcpState unexpected action error");
        };
    auto none = [](TcpInternalInterface &internal) {};
    
    tranform_rule_.resize(static_cast<int>(St::kTimeWait)+2);
    tranform_rule_[0] = { // State::kClosed
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

    tranform_rule_[1] = {};  // State::kListen

    tranform_rule_[2] = {  // State::kSynRcvd
        std::make_tuple(Ev::kAckRecv, St::kEstab, Sg::kEstab,
            full_check, full_update, none, send_rst),
        std::make_tuple(Ev::kFinSent, St::kFinWait1, Sg::kClosing,
            empty_check, empty_update, send_fin, nope)}; 
        
    tranform_rule_[3] = { // State::kSynSent
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
        
    tranform_rule_[4] = { // State::kEstab
        std::make_tuple(Ev::kAckRecv, St::kEstab, Sg::kEstab,
            full_check, full_update, send_cond_ack, discard),
        std::make_tuple(Ev::kFinRecv, St::kCloseWait, Sg::kClosing,
            full_check, fin_update, response_fin, discard),
        std::make_tuple(Ev::kFinSent, St::kFinWait1, Sg::kClosing,
            empty_check, empty_update, send_fin, nope)};
    
    tranform_rule_[5] = { // State::kFinWait1
        std::make_tuple(Ev::kAckRecv, St::kFinWait2, Sg::kClosing, // FinAck
            finack_check, full_update, send_cond_ack, discard),
        std::make_tuple(Ev::kFinRecv, St::kClosing, Sg::kClosing,
            full_check, fin_update, response_fin, discard)};
    
    tranform_rule_[6] = { // State::kCloseWait
        std::make_tuple(Ev::kFinSent, St::kLastAck, Sg::kClosing,
            empty_check, empty_update, none, nope)};
    
    tranform_rule_[7] = { // State::kFinWait2
        // skip kTimeWait
        std::make_tuple(Ev::kAckRecv, St::kFinWait2, Sg::kClosing,
            full_check, full_update, send_cond_ack, discard),
        std::make_tuple(Ev::kFinRecv, St::kClosed, Sg::kClosed,
            full_check, fin_update, close, discard)};
    
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

std::function<void(TcpInternalInterface &)> TcpStateMachine::OnReceivePackage(
    const TcpHeader *header, uint16_t size) {
  assert(header);
  
  auto event = ParseEvent(header);
  std::cerr << __func__ << ": " << event << " ";
  constexpr int Check = 3;
  constexpr int Update = 4;
  constexpr int Success = 5;
  constexpr int Failed = 6;
  
  last_package_size_ = size;
  
  // Event::kRst State::Listen
  auto sepcial_rule_result = SpecialRule(event, header, size);
  if (sepcial_rule_result.second) { 
    std::cerr << "SpecialRule applied" << std::endl;
    return sepcial_rule_result.first;
  } 
  
  const auto &rule = tranform_rule_[static_cast<int>(state_)];
  
  for (auto &condition : rule) {
    if (std::get<0>(condition) == event) {
      auto check_result = std::get<Check>(condition)(*this, header, size);
      std::cerr << "Check Result: " << check_result << " ";
      if (check_result == 0) {
        state_ = std::get<1>(condition);
        stage_ = std::get<2>(condition);

        std::get<Update>(condition)(*this, header, size);
        return std::get<Success>(condition);
        
      } else if (check_result > 0) {
        std::get<Update>(condition)(*this, header, size);
        return std::get<Success>(condition);
        
      } else {
        return std::get<Failed>(condition);
      }
    }
  }
  
  std::cerr << " end" << std::endl;
  return std::get<Failed>(tranform_rule_.back().front()); // default
}

} // namespace tcp_simulator
