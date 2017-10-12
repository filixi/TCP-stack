#ifndef _TCP_STACK_STATE_MACHINE_H_
#define _TCP_STACK_STATE_MACHINE_H_

#include <cassert>
#include <cstddef>

#include <functional>
#include <iterator>
#include <utility>
#include <variant>

#include <iostream>

#include "tcp-header.h"

namespace tcp_stack {
enum class State {
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
  kTimeWait
};

inline const char *ToString(State state) {
  static const char names[][32] = {
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
  assert(static_cast<int>(state) >= 0 &&
      static_cast<int>(state) < static_cast<std::ptrdiff_t>(std::size(names)));
  return names[static_cast<int>(state)];
}

enum class Event {
  kListen = 0,
  kConnect,
  kSend,
  kClose
};

class SocketInternalInterface {
public:
  virtual void SendSyn(uint32_t seq, uint16_t window) = 0;
  virtual void SendSynAck(uint32_t seq, uint32_t ack, uint16_t window) = 0;
  virtual void SendAck(uint32_t seq, uint32_t ack, uint16_t window) = 0;
  virtual void SendFin(uint32_t seq, uint32_t ack, uint16_t window) = 0;

  virtual void RecvSyn(uint32_t seq_recv, uint16_t window_recv) = 0;
  virtual void RecvAck(
      uint32_t seq_recv, uint32_t ack_recv, uint16_t window_recv) = 0;
  virtual void RecvFin(
      uint32_t seq_recv, uint32_t ack_recv, uint16_t window_recv) = 0;

  virtual void Listen() = 0;
  virtual void Connected() = 0;

  virtual void Accept() = 0;
  virtual void Discard() = 0;
  virtual void SeqOutofRange(uint16_t window) = 0;
  virtual void SendRst(uint32_t seq) = 0;

  virtual void InvalidOperation() = 0;

  virtual void NewConnection() = 0;
  virtual void Close() = 0;
  virtual void TimeWait() = 0;
};

struct TcpControlBlock;

class TcpState {
public:
  using ReactType = std::function<void(SocketInternalInterface *)>;
  using TriggerType = std::pair<ReactType, TcpState *>;

  virtual ~TcpState() = default;

  virtual TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) = 0;
  virtual TriggerType operator()(const TcpHeader &, TcpControlBlock &) = 0;

  virtual State GetState() const = 0;
};

class Closed final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kClosed;
  }
};

class Listen final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kListen;
  }
};

class SynRcvd final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kSynRcvd;
  }
};

class SynSent final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kSynSent;
  }
};

class Estab final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kEstab;
  }
};

class FinWait1 final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kFinWait1;
  }
};

class CloseWait final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kCloseWait;
  }
};

class FinWait2 final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kFinWait2;
  }
};

class Closing final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kClosing;
  }
};

class LastAck final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kLastAck;
  }
};

class TimeWait final : public TcpState {
public:
  TriggerType operator()(Event, TcpHeader *, TcpControlBlock &) override;

  TriggerType operator()(const TcpHeader &, TcpControlBlock &) override;

  State GetState() const override {
    return State::kTimeWait;
  }
};

struct TcpControlBlock {
  uint32_t snd_seq = 0; // initial sequence number

  uint32_t snd_una = 0; // oldest unacknowledge number
  uint32_t snd_nxt = 0; // next sequence number to send
  uint16_t snd_wnd = 0; // window

  uint32_t rcv_nxt = 0; // next sequence number to recv
  uint32_t rcv_wnd = 0; // windows

  std::variant<Closed, Listen, SynRcvd, SynSent, Estab, FinWait1, CloseWait,
               FinWait2, Closing, LastAck, TimeWait> state;
};

class TcpStateManager {
public:
  TcpStateManager() = default;

  TcpStateManager(const TcpStateManager &x)
      : block_(x.block_),
        state_(std::visit([](auto &arg) {
            return static_cast<TcpState *>(&arg);
          }, block_.state)) {}
  
  TcpStateManager &operator=(const TcpStateManager &) = delete;

  TcpState::ReactType operator()(Event event, TcpHeader *header) {
    std::cout << "Action In: " << ToString(GetState()) << std::endl;

    auto [react, new_state] = state_->operator()(event, header, block_);
    state_ = new_state;

    std::cout << "Action out: " << ToString(GetState()) << std::endl;
    return react;
  }

  TcpState::ReactType operator()(const TcpHeader &header) {
    std::cout << this << " Packet In: " << ToString(GetState()) << std::endl;

    auto [react, new_state] = state_->operator()(header, block_);
    state_ = new_state;

    std::cout << "Packet Out: " << ToString(GetState()) << std::endl;
    return react;
  }

  TcpState::ReactType InvalideCheckSum() {
    const auto &b = block_;
    return [seq = b.snd_nxt, ack = b.rcv_nxt, wnd = b.snd_wnd](
            SocketInternalInterface *tcp) {
          tcp->Discard();
          tcp->SendAck(seq, ack, wnd);
        };
  }

  State GetState() const {
    return state_->GetState();
  }

  const auto &GetControlBlock() const {
    return block_;
  }

  auto &Window() {
    return block_.snd_wnd;
  }
  auto &Window() const {
    return block_.snd_wnd;
  }

  auto PeerWindow() const {
    return block_.rcv_wnd;
  }

  auto GetNextSend() const {
    return block_.snd_nxt;
  }

  void Reset() {
    block_ = TcpControlBlock();
    state_ = &std::get<Closed>(block_.state);
  }

private:
  TcpControlBlock block_;
  TcpState *state_ = &std::get<Closed>(block_.state);
};

} // namespace tcp_stack

#endif // _TCP_STACK_STATE_MACHINE_H_
