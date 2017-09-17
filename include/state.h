#ifndef _TCP_STATE_MACHINE_STATE_H_
#define _TCP_STATE_MACHINE_STATE_H_

#include <cassert>

#include <functional>
#include <iterator>
#include <utility>
#include <variant>

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
  assert(int(state) >= 0 && int(state)<std::size(names));
  return names[int(state)];
}

enum class Event {
  kListen = 0,
  kConnect,
  kSend,
  kClose
};

template <size_t size>
class Field {
 public:
  static constexpr size_t kSize = size;
  
  Field() : field_{0} {}
  
  uint8_t GetAtBit(size_t pos) const {
    assert(pos < size*32);
    return field_[pos/32] & (1u<<pos%32)?1:0;
  }
  
  void SetAtBit(size_t pos, bool value) {
    assert(pos < size*32);
    if (value)
      field_[pos/32] |= (1u<<pos%32);
    else
      field_[pos/32] &= ~(1u<<pos%32); 
  }
  
  template <class T>
  T &At(size_t pos) {
    return const_cast<T &>(
        static_cast<const Field *>(this)->At<T>(pos));
  }
  template <class T>
  const T &At(size_t pos) const {
    assert(pos < size*32);
    assert(pos%8 == 0);
    assert(pos/8%alignof(T) == 0);
    
    return reinterpret_cast<const T &>(
        reinterpret_cast<const uint8_t *>(field_)[pos/8]);
  }
  
 private:
  uint32_t field_[size];
};

class TcpHeader {
 public:
  uint32_t &SourceAddress() {
    return const_cast<uint32_t &>(
        static_cast<const TcpHeader *>(this)->SourceAddress());
  }
  const uint32_t &SourceAddress() const {
    return prefixed_field_.At<uint32_t>(0);
  }
  
  uint32_t &DestinationAddress() {
    return const_cast<uint32_t &>(
        static_cast<const TcpHeader *>(this)->DestinationAddress());
  }
  const uint32_t &DestinationAddress() const {
    return prefixed_field_.At<uint32_t>(32);
  }
  
  // zero
  
  uint8_t &PTCL() {
    return const_cast<uint8_t &>(
        static_cast<const TcpHeader *>(this)->PTCL());
  }
  const uint8_t &PTCL() const {
    return prefixed_field_.At<uint8_t>(72);
  }
  
  uint16_t &TcpLength() {
    return const_cast<uint16_t &>(
        static_cast<const TcpHeader *>(this)->TcpLength());
  }
  const uint16_t &TcpLength() const {
    return prefixed_field_.At<uint16_t>(80);
  }
  
  uint16_t &SourcePort() {
    return const_cast<uint16_t &>(
        static_cast<const TcpHeader *>(this)->SourcePort());
  }
  const uint16_t &SourcePort() const {
    return field_.At<uint16_t>(0);
  }
  
  uint16_t &DestinationPort() {
    return const_cast<uint16_t &>(
        static_cast<const TcpHeader *>(this)->DestinationPort());
  }
  const uint16_t &DestinationPort() const {
    return field_.At<uint16_t>(16);
  }
  
  uint32_t &SequenceNumber() {
    return const_cast<uint32_t &>(
        static_cast<const TcpHeader *>(this)->SequenceNumber());
  }
  const uint32_t &SequenceNumber() const {
    return field_.At<uint32_t>(32);
  }
  
  uint32_t &AcknowledgementNumber() {
    return const_cast<uint32_t &>(
        static_cast<const TcpHeader *>(this)->AcknowledgementNumber());
  }
  const uint32_t &AcknowledgementNumber() const {
    return field_.At<uint32_t>(64);
  }
  
  // Data Offset and Reserved
  
  bool Urg() const {
    return field_.GetAtBit(106);
  }
  void SetUrg(bool value) {
    return field_.SetAtBit(106, value);
  }
  
  bool Ack() const {
    return field_.GetAtBit(107);
  }
  void SetAck(bool value) {
    return field_.SetAtBit(107, value);
  }
  
  bool Psh() const {
    return field_.GetAtBit(108);
  }
  void SetPsh(bool value) {
    return field_.SetAtBit(108, value);
  }
  
  bool Rst() const {
    return field_.GetAtBit(109);
  }
  void SetRst(bool value) {
    return field_.SetAtBit(109, value);
  }
  
  bool Syn() const {
    return field_.GetAtBit(110);
  }
  void SetSyn(bool value) {
    return field_.SetAtBit(110, value);
  }
  
  bool Fin() const {
    return field_.GetAtBit(111);
  }
  void SetFin(bool value) {
    return field_.SetAtBit(111, value);
  }
  
  uint16_t &Window() {
    return const_cast<uint16_t &>(
        static_cast<const TcpHeader *>(this)->Window());
  }
  const uint16_t &Window() const {
    return field_.At<uint16_t>(112);
  }
  
  uint16_t &Checksum() {
    return const_cast<uint16_t &>(
        static_cast<const TcpHeader *>(this)->Checksum());
  }
  const uint16_t &Checksum() const {
    return field_.At<uint16_t>(128);
  }
  
  uint16_t &UrgentPointer() {
    return const_cast<uint16_t &>(
        static_cast<const TcpHeader *>(this)->UrgentPointer());
  }
  const uint16_t &UrgentPointer() const {
    return field_.At<uint16_t>(142);
  }
  
  bool Special() const {
    return Psh() || Rst() || Syn() || Fin() || Urg();
  }
  
 private:
  Field<3> prefixed_field_;
  Field<5> field_;
};

class TcpInternalInterface {
 public:
  virtual void SendSyn(uint32_t seq, uint32_t window) = 0;
  virtual void SendSynAck(uint32_t seq, uint32_t ack, uint32_t window) = 0;
  virtual void SendAck(uint32_t seq, uint32_t ack, uint32_t window) = 0;
  virtual void SendFin(uint32_t seq, uint32_t ack, uint32_t window) = 0;

  virtual void RecvSyn(uint32_t seq_recv, uint32_t window_recv) = 0;
  virtual void RecvAck(
      uint32_t seq_recv, uint32_t ack_recv, uint32_t window_recv) = 0;
  virtual void RecvFin(
      uint32_t seq_recv, uint32_t ack_recv, uint32_t window_recv) = 0;

  virtual void Accept() = 0;
  virtual void Discard() = 0;
  virtual void SeqOutofRange(uint32_t window) = 0;
  virtual void SendRst(uint32_t seq) = 0;

  virtual void InvalidOperation() = 0;

  virtual void NewConnection() = 0;
};

struct TcpControlBlock;

class TcpState {
 public:
  using ReactType = std::function<void(TcpInternalInterface *)>;
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
  using int_type = uint32_t;
  int_type snd_seq; // initial sequence number

  int_type snd_una; // oldest unacknowledge number
  int_type snd_nxt; // next sequence number to send
  int_type snd_wnd; // window

  int_type rcv_nxt; // next sequence number to recv
  int_type rcv_wnd; // windows

  std::variant<Closed, Listen, SynRcvd, SynSent, Estab, FinWait1, CloseWait,
               FinWait2, Closing, LastAck, TimeWait> state;
};

class TcpStateManager {
 public:
  TcpState::ReactType operator()(Event event, TcpHeader *header) {
    auto [react, new_state] = state_->operator()(event, header, block_);
    state_ = new_state;
    return react;
  }

  TcpState::ReactType operator()(const TcpHeader &header) {
    auto [react, new_state] = state_->operator()(header, block_);
    state_ = new_state;
    return react;
  }

 private:
  TcpControlBlock block_;
  TcpState *state_;
};

#endif // _TCP_STATE_MACHINE_STATE_H_
