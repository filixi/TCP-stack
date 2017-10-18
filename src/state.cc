#include "state.h"

#include <iostream>
#include <random>

namespace tcp_stack {
namespace {
inline bool IsAck(const TcpHeader &header) {
  return header.Ack() && !header.Syn() && !header.Fin();
}

inline bool IsSyn(const TcpHeader &header) {
  return header.Syn() && !header.Ack() && !header.Fin();
}

inline bool IsSynAck(const TcpHeader &header) {
  return header.Syn() && header.Ack() && !header.Fin();
}

inline bool IsFin(const TcpHeader &header) {
  return header.Fin() && header.Ack() && !header.Syn();
}

inline uint32_t RandomSynNumber() {
  thread_local static std::mt19937 e(std::random_device{}());
  thread_local static std::uniform_int_distribution<uint32_t> d(10, 10000);

  return d(e);
}

} // anonymous namespace

Closed::TriggerType Closed::operator()(
    Event event, TcpHeader *, TcpControlBlock &b) {
  if (event == Event::kListen) {
    return {[](SocketInternalInterface *tcp) {
          tcp->Listen();
        }, &b.state.emplace<Listen>()};
  } else if (event == Event::kConnect) {
    b.snd_seq = RandomSynNumber();
    b.snd_una = b.snd_seq;
    b.snd_nxt = b.snd_seq + 1;
    b.snd_wnd = 1024;
    return {[seq = b.snd_seq, wnd = b.snd_wnd](SocketInternalInterface *tcp) {
          tcp->SendSyn(seq, wnd);
        }, &b.state.emplace<SynSent>()};
  } else if (event == Event::kClose) {
    return {[](SocketInternalInterface *tcp) {tcp->Close();}, this};
  }

  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

Closed::TriggerType Closed::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  if (IsSyn(header)) {
    b.snd_seq = RandomSynNumber();
    b.snd_una = b.snd_seq;
    b.snd_nxt = b.snd_seq + 1;
    b.snd_wnd = 1024;

    b.rcv_nxt = header.SequenceNumber() + 1;
    b.rcv_wnd = header.Window();

    return {[seq = b.snd_seq, ack = b.rcv_nxt, wnd = b.snd_wnd](
            SocketInternalInterface *tcp) {
          tcp->Accept();
          tcp->SendSynAck(seq, ack, wnd);
        }, &b.state.emplace<SynRcvd>()};
  }

  return {[seq = header.AcknowledgementNumber()](SocketInternalInterface *tcp) {
        tcp->Discard();
        tcp->SendRst(seq);
      }, this};
}

Listen::TriggerType Listen::operator()(
    Event event, TcpHeader *, TcpControlBlock &b) {
  // Initiate connection from Listen is forbiden.
  if (event == Event::kClose) {
    return {[](SocketInternalInterface *tcp) {tcp->Close();},
            &b.state.emplace<Closed>()};
  } else {
    return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
  }
}

Listen::TriggerType Listen::operator()(
    const TcpHeader &header, TcpControlBlock &) {
  if (header.Syn() && !header.Ack()) {
    return {[](SocketInternalInterface *tcp) {
          tcp->Accept();
          tcp->NewConnection();
        }, this};
  }

  // Send Rst ?
  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

SynRcvd::TriggerType SynRcvd::operator()(
    Event event, TcpHeader *, TcpControlBlock &b) {
  if (event == Event::kClose) {
    return {[seq = b.snd_nxt++, ack = b.rcv_nxt, wnd = b.snd_wnd](
            SocketInternalInterface *tcp){
          tcp->SendFin(seq, ack, wnd);
        }, &b.state.emplace<FinWait1>()};
  }

  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

SynRcvd::TriggerType SynRcvd::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  if (IsAck(header)) {
    if (header.AcknowledgementNumber() == b.snd_nxt) {
      b.snd_una = header.AcknowledgementNumber();
      b.rcv_wnd = header.Window();
      return {[](SocketInternalInterface *tcp) {
                tcp->Accept();
                tcp->Connected();
              }, &b.state.emplace<Estab>()};
    } else {
      // Send Syn ?
    }
  }

  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

SynSent::TriggerType SynSent::operator()(
    Event, TcpHeader *, TcpControlBlock &) {
  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

SynSent::TriggerType SynSent::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  if (IsSyn(header)) {
    b.rcv_nxt = header.SequenceNumber() + 1;
    b.rcv_wnd = header.Window();
    return {[seq = b.snd_nxt - 1, ack = b.rcv_nxt, wnd = b.snd_wnd](
            SocketInternalInterface *tcp) {
          tcp->Accept();
          tcp->SendAck(seq, ack, wnd);
        }, &b.state.emplace<SynRcvd>()};
  } else if (IsSynAck(header)) {
    if (header.AcknowledgementNumber() == b.snd_nxt) {
      b.snd_una = header.AcknowledgementNumber();

      b.rcv_nxt = header.SequenceNumber() + 1;
      b.rcv_wnd = header.Window();
      return {[seq = b.snd_nxt, ack = b.rcv_nxt, wnd = b.snd_wnd](
              SocketInternalInterface *tcp) {
            tcp->Accept();
            tcp->SendAck(seq, ack, wnd);
            tcp->Connected();
          }, &b.state.emplace<Estab>()};
    } else {
      // Send Syn ?
    }
  }

  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

Estab::TriggerType Estab::operator()(
    Event event, TcpHeader *header, TcpControlBlock &b) {
  if (event == Event::kSend) {
    assert(header);

    // need check
    if (b.snd_nxt + header->TcpLength() >= b.snd_una + b.snd_wnd)
      return {[wnd = b.snd_wnd](SocketInternalInterface *tcp) {
            tcp->SeqOutofRange(wnd);
          }, this};

    header->SetAck(true);
    header->SequenceNumber() = b.snd_nxt;
    header->AcknowledgementNumber() = b.rcv_nxt;

    b.snd_nxt += header->TcpLength();

    return {[](auto) {}, this};
  } else if (event == Event::kClose) {
    return {[seq = b.snd_nxt++, ack = b.rcv_nxt, wnd = b.snd_wnd](
            SocketInternalInterface *tcp) {
          tcp->SendFin(seq, ack, wnd);
        }, &b.state.emplace<FinWait1>()};
  }

  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

Estab::TriggerType Estab::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  if (IsAck(header) &&
      header.AcknowledgementNumber() <= b.snd_nxt &&
      header.SequenceNumber() == b.rcv_nxt) {
    b.snd_una = std::max(header.AcknowledgementNumber(), b.snd_una);
    b.rcv_nxt = header.SequenceNumber() + header.TcpLength();
    b.rcv_wnd = header.Window();
    return {[seq = b.snd_nxt, ack = b.rcv_nxt, send_ack = header.TcpLength(),
             peer_ack = header.AcknowledgementNumber(), wnd = b.snd_wnd](
            SocketInternalInterface *tcp) {
          tcp->Accept();
          tcp->RecvAck(seq, peer_ack, wnd);
          if (send_ack)
            tcp->SendAck(seq, ack, wnd);
        }, this};
  } else if (IsFin(header) &&
             header.AcknowledgementNumber() <= b.snd_nxt &&
             header.SequenceNumber() == b.rcv_nxt) { // need check
    b.rcv_wnd = header.Window();
    return {[seq = b.snd_nxt, ack = ++b.rcv_nxt, wnd = b.snd_wnd](
            SocketInternalInterface *tcp) {
          tcp->Accept();
          tcp->SendAck(seq, ack, wnd);
        }, &b.state.emplace<CloseWait>()};
  }

  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

FinWait1::TriggerType FinWait1::operator()(
    Event, TcpHeader *, TcpControlBlock &) {
  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

FinWait1::TriggerType FinWait1::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  if (IsAck(header) &&
      header.AcknowledgementNumber() <= b.snd_nxt &&
      header.SequenceNumber() == b.rcv_nxt) {
    if (header.AcknowledgementNumber() == b.snd_nxt) // ack fin
      return {[](SocketInternalInterface *tcp) { tcp->Accept(); },
              &b.state.emplace<FinWait2>()};
    else
      return {[](SocketInternalInterface *tcp) { tcp->Accept(); }, this};
  } else if (IsFin(header) && 
             header.AcknowledgementNumber() <= b.snd_nxt &&
             header.SequenceNumber() == b.rcv_nxt) {
    b.rcv_wnd = header.Window();
    if (header.AcknowledgementNumber() == b.snd_nxt) {
      // ACK from the other side is combined with the FIN header
      return {[seq = b.snd_nxt, ack = ++b.rcv_nxt, wnd = b.snd_wnd](
              SocketInternalInterface *tcp) {
            tcp->Accept();
            tcp->SendAck(seq, ack, wnd);
            tcp->TimeWait();
          }, &b.state.emplace<TimeWait>()};
    } else {
      return {[seq = b.snd_nxt, ack = ++b.rcv_nxt, wnd = b.snd_wnd](
              SocketInternalInterface *tcp) {
            tcp->Accept();
            tcp->SendAck(seq, ack, wnd);
          }, &b.state.emplace<Closing>()};
    }
  }

  return {[](SocketInternalInterface *tcp) { tcp->Discard(); }, this};
}

CloseWait::TriggerType CloseWait::operator()(
    Event event, TcpHeader *, TcpControlBlock &b) {
  if (event == Event::kClose) {
    return {[seq = b.snd_nxt++, ack = b.rcv_nxt, wnd = b.snd_wnd](
            SocketInternalInterface *tcp) {
          tcp->SendFin(seq, ack, wnd);
        }, &b.state.emplace<LastAck>()};
  }
  
  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

CloseWait::TriggerType CloseWait::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  // if (IsAck(header) && IsSeqAckInRange(header, b)) {
  //   b.snd_una = header.AcknowledgementNumber();
  //   b.rcv_nxt = header.SequenceNumber();
  //   b.rcv_wnd = header.Window();

  //   return {[](SocketInternalInterface *tcp) {tcp->Accept();}, this};
  // }

  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

FinWait2::TriggerType FinWait2::operator()(
    Event, TcpHeader *, TcpControlBlock &) {
  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

FinWait2::TriggerType FinWait2::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  if (IsFin(header) && 
      header.AcknowledgementNumber() == b.snd_nxt &&
      header.SequenceNumber() == b.rcv_nxt) {
    b.rcv_nxt = header.SequenceNumber() + 1;
    b.rcv_wnd = header.Window();

    return {[seq = b.snd_nxt, ack = b.rcv_nxt, wnd = b.snd_wnd](
            SocketInternalInterface *tcp) {
          tcp->Accept();
          tcp->SendAck(seq, ack, wnd);
          tcp->TimeWait();
        }, &b.state.emplace<TimeWait>()};
  }

  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

Closing::TriggerType Closing::operator()(
    Event, TcpHeader *, TcpControlBlock &) {
  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

Closing::TriggerType Closing::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  if (IsAck(header) &&
      header.AcknowledgementNumber() >= b.snd_una &&
      header.SequenceNumber() == b.rcv_nxt) {
    b.snd_una = header.AcknowledgementNumber();
    if (b.snd_nxt == header.AcknowledgementNumber())
      return {[](SocketInternalInterface *tcp) {
                tcp->Accept();
                tcp->TimeWait();
              }, &b.state.emplace<TimeWait>()};
    else
      return {[](SocketInternalInterface *tcp) {
                tcp->Accept();
              }, this};
  }

  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

LastAck::TriggerType LastAck::operator()(
    Event, TcpHeader *, TcpControlBlock &) {
  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

LastAck::TriggerType LastAck::operator()(
    const TcpHeader &header, TcpControlBlock &b) {
  if (IsAck(header) &&
      header.AcknowledgementNumber() >= b.snd_una &&
      header.SequenceNumber() == b.rcv_nxt) {
    b.snd_una = header.AcknowledgementNumber();
    if (b.snd_nxt == header.AcknowledgementNumber())
      return {[](SocketInternalInterface *tcp) {
                tcp->Accept();
                tcp->Close();
              }, &b.state.emplace<Closed>()};
    else
      return {[](SocketInternalInterface *tcp) {
                tcp->Accept();
              }, this};
  }
  
  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

TimeWait::TriggerType TimeWait::operator()(
    Event, TcpHeader *, TcpControlBlock &) {
  return {[](SocketInternalInterface *tcp) {tcp->InvalidOperation();}, this};
}

TimeWait::TriggerType TimeWait::operator()(
    const TcpHeader &, TcpControlBlock &) {
  return {[](SocketInternalInterface *tcp) {tcp->Discard();}, this};
}

} // namespace tcp_stack
