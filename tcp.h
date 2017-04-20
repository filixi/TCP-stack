#ifndef _TCP_H_
#define _TCP_H_

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "tcp-buffer.h"
#include "tcp-state-machine.h"

class TcpSocket;
class TcpManager;

template <class Fn>
void MakeFunctionVectorImpl(std::vector<std::function<int(int)> > &vtr,
  Fn &&fn) {
  vtr.emplace_back(static_cast<std::function<int(int)> >(fn));
}

template <class Fn, class... Fns>
void MakeFunctionVectorImpl(std::vector<std::function<int(int)> > &vtr,
                            Fn &&fn, Fns&&... fns) {
  vtr.emplace_back(static_cast<std::function<int(int)> >(fn));
}

template <class... Fns>
std::vector<std::function<int(int)> > MakeFunctionVector(Fns&&... fns) {
  std::vector<std::function<int(int)> > result;
  MakeFunctionVectorImpl(result, std::forward<Fns>(fns)...);
  return result;
}

class TcpInternal {
 public:
  friend struct DebugTcp;
  
  TcpInternal(uint64_t id, TcpManager *manager, uint16_t host_port,
              uint16_t peer_port)
      : id_(id), manager_(manager), host_port_(host_port),
        peer_port_(peer_port) {
    actions_ = MakeFunctionVector(
        [this](int){return 0;},                 // kNone = 0,
        [this](int){SendSyn(); return 0;},      // kSendSyn,
        [this](int size) {
          if (size > 0)
            SendAck();
          return 0;
        },      // kSendAck,
        [this](int){SendSynAck(); return 0;},   // kSendSynAck,
        [this](int){return 0;},                 // kClose,
        [this](int){return 0;},                 // kWaitAck,
        [this](int){return 0;}                  // kWaitFin
    );
    
  }
  
  TcpInternal(const TcpInternal &) = delete;
  TcpInternal(TcpInternal &&) = default;
  
  TcpInternal &operator=(const TcpInternal &) = delete;
  TcpInternal &operator=(TcpInternal &&) = delete;
  
  int Listen() {
    return 0;
  }
  
  void OnConnect() {
    
  }
  
  int CloseInternal();  
  TcpSocket AcceptConnection();
  
  std::list<TcpPackage> GetReceivedPackages() {
    return buffer_.GetReadPackages();
  }
  
  std::list<std::shared_ptr<NetworkPackage> > GetPackagesForSending() {
    return buffer_.GetWritePackages(
        peer_window_ - (host_next_seq_ - peer_last_ack_),
        [this](TcpPackage &pack) mutable {
          PreprePackageForSending(pack);
        });
  }
  
  std::list<std::shared_ptr<NetworkPackage> > GetPackagesForResending() {
    return buffer_.GetPackagesForResending(
        [this](TcpPackage &pack) mutable {
          PreprePackageForSending(pack);
        });
  }
  
  int AddPackageForSending(TcpPackage package) {
    buffer_.AddToWriteBuffer(std::move(package));
    return 0;
  }
  
  int ReceivePackage(TcpPackage package) {
    assert(state_ != Stage::kClosed);
    
    if (!package.ValidByChecksum())
      return -1000;

    auto check_result = HeaderCheckUpdate(package);
    if (check_result == 1) // NewConnection
      return 1;
    if (check_result == 2) // RstRecv
      return 2;
    else if (check_result) {
      SendAck();
      return check_result; // Discard
    }
    
    buffer_.Ack(package.GetHeader().SequenceNumber());
    auto event = package.GetHeader().ParseEvent();
    auto package_size = package.Length();
    if (state_ == Stage::kEstab) {
      unsequenced_packages_.emplace(package.GetHeader().SequenceNumber(),
                                    std::move(package));
      EstabUpdateState();
    }

    std::cerr << __func__ << ": unseq_packsSize:" << unsequenced_packages_.size() << std::endl;
    
    auto action = state_.OnEvent(event);
    std::cerr << __func__ << ": state_.OnEvent() result : " << action << std::endl;
    
    std::cerr << __func__ << ": action int: " << static_cast<int>(action) << std::endl;
    return actions_.at(static_cast<int>(action))(package_size);

    throw std::runtime_error("TcpInternal::ReceivedPackage error");
  }
  
  auto Id() const {
    return id_;
  }
  
 private:
  void PreprePackageForSending(TcpPackage &package) {
    auto &header = package.GetHeader();
    if (!header.Syn() && !header.Rst() && !header.Fin())
      new(&header) TcpHeader();
    
    header.SourcePort() = host_port_;
    header.DestinationPort() = peer_port_;
    
    if (!header.Syn()) {
      header.SequenceNumber() = host_next_seq_;
      host_next_seq_ += package.Length();
      
      header.AcknowledgementNumber() = host_last_ack_;
      header.SetAck(true);
    }

    header.Window() = host_window_;
    header.Checksum() = 0;
    header.Checksum() = package.CalculateChecksum();
  }
  
  void SendSyn() {
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SequenceNumber() = host_initial_seq_;
    package.GetHeader().SetSyn(true);
    AddPackageForSending(std::move(package));
  }
  
  void SendAck() {
    TcpPackage package(nullptr, nullptr);
    AddPackageForSending(std::move(package));
  }
  
  void SendSynAck() {
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SequenceNumber() = host_initial_seq_;
    package.GetHeader().SetSyn(true);
    package.GetHeader().SetAck(true);
    AddPackageForSending(std::move(package));
  }
  
  void SendRst() {
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetRst(true);
    AddPackageForSending(std::move(package));
  }
  
  void SendFin() {
    TcpPackage package(nullptr, nullptr);
    package.GetHeader().SetFin(true);
    AddPackageForSending(std::move(package));
  }
  
  // all peer
  int HeaderCheckUpdate(const TcpPackage &package) {
    const auto &header = package.GetHeader();
    auto seq = header.SequenceNumber();
    auto ack = header.AcknowledgementNumber();
    auto len = package.Length();
    auto window = header.Window();
    auto event = header.ParseEvent();
    
    if (event == Event::kSynRecv) {
      if (state_ != Stage::kHandShake) {
        return -1;
      } else if (state_ == State::kListen) {
        return 1;
      } else if (state_ == State::kSynSent) {
        // update
        peer_initial_seq_ = seq;
        state_.OnEvent(event);
        return 0;
      }
      return -2;
      
    } else if (event == Event::kAckRecv) {
      if (state_ == State::kSynRcvd ) {
        if (seq != host_last_ack_)
          return -3;
        if (ack != host_next_seq_)
          return -4;
        peer_last_ack_ = ack;
        peer_window_ = window;
        return 0;
        
      } else if (state_ == State::kEstab ||
                 state_ == State::kFinWait1 ||
                 state_ == State::kClosing ||
                 state_ == State::kLastAck) {
        // check seq/ack
        auto ret = AckSeqCheck(seq, ack, len);
        if (ret == 0) {
          peer_last_ack_ = ack;
          peer_window_ = window;
        }
        return ret;
      }
      return -5;
      
    } else if (event == Event::kSynAckRecv) {
      if (state_ == State::kSynSent) {
        // check seq/ack
        auto ret = AckSeqCheck(seq, ack, len);
        if (ret == 0) {
          peer_last_ack_ = ack;
          host_last_ack_ = seq+1;
          peer_window_ = window;
        }
        return ret;
      }
      return -6;
      
    } else if (event == Event::kFinRecv) {
      if (state_ == State::kSynRcvd ||
          state_ == State::kEstab ||
          state_ == State::kFinWait1 ||
          state_ == State::kFinWait2) {
        auto ret = AckSeqCheck(seq, ack, len);
        if (ret == 0) {
          peer_last_ack_ = ack;
          peer_window_ = window;
        }
        return ret;
      }
      return -7;
      
    } else if (event == Event::kRst) {
      if (seq == host_last_ack_)
        return 2;
      return -8;
      
    } else if (event == Event::kFinAckRecv) {
      ;
      
    } else if (event == Event::kNone) {
      return -11;
    } else {
      std::cerr << static_cast<int>(event) << std::endl;
      throw std::runtime_error("TcpInternal::HeaderCheckUpdate failed");
    }
    
    if (event == Event::kFinRecv) {
      if (state_ != State::kSynRcvd &&
          state_ != State::kEstab &&
          state_ != State::kFinWait1)
        return -9;
      if (seq < host_last_ack_)
        return -10;
      return 0;
    }
    
    throw std::runtime_error("TcpInternal::HeaderCheck State Error");
  }
  
  int AckSeqCheck(uint32_t seq, uint32_t ack, size_t len) {
    if (seq < peer_initial_seq_)
      return -101;
    if (seq+len > host_last_ack_+host_window_)
      return -102;
    if (seq < host_last_ack_)
      return -103;
    if (ack < host_initial_seq_)
      return -104;
    if (ack > host_next_seq_)
      return -105;
    
    return 0;
  }
  
  void EstabUpdateState() {
    auto ite = unsequenced_packages_.begin();
    while (ite != unsequenced_packages_.end()) {
      if (ite->first == host_last_ack_) {
        const auto &package = ite->second;
        host_last_ack_ = package.GetHeader().SequenceNumber() +
            package.Length();

        buffer_.AddToReadBuffer(std::move(ite->second));
      } else if (ite->first > host_last_ack_) {
        break;
      }
      
      unsequenced_packages_.erase(ite);
      ++ite;
    }
  }
  
  uint64_t id_;
  
  TcpBuffer buffer_;
  TcpStateMachine state_;
  
  TcpManager *manager_;
  
  uint32_t peer_initial_seq_; // HeaderCheckUpdate
  uint32_t peer_last_ack_; // HeaderCheckUpdate
  uint32_t peer_window_; // HeaderCheckUpdate
  
  uint32_t host_initial_seq_ = 10; // none
  uint32_t host_next_seq_ = host_initial_seq_+1; // GetPackagesForSending
  uint32_t host_last_ack_; // EstabUpdateState
  uint32_t host_window_ = 4096; // none
  
  uint16_t host_port_;
  uint16_t peer_port_;
  
  std::map<uint32_t, TcpPackage> unsequenced_packages_;
  
  std::vector<std::function<int(int)> > actions_;
};

class TcpSocket {
 public:
  TcpSocket() {}
  TcpSocket(std::weak_ptr<TcpInternal> internal) : internal_(internal) {}
  
  int Listen() {
    if (internal_.expired())
      return -1;
    return internal_.lock()->Listen();
  }
  
  std::pair<bool, TcpSocket> Accept() {
    if (internal_.expired())
      return {false, {}};
    return {true, internal_.lock()->AcceptConnection()};
  }
  
  TcpSocket Connect() {
    // interrupt pselect
    
    return {};
  }
  
  auto Read() {
    using PackagesType = std::list<TcpPackage>;
    if (internal_.expired())
      return std::make_pair(false, PackagesType{});
    return std::make_pair(true, internal_.lock()->GetReceivedPackages());
  }
  
  // haven't
  int Write(const char *first, const char *last) {
    return 0;
  }
  
  int Close() {
    if (internal_.expired())
      return -1;
    
    auto ret = internal_.lock()->CloseInternal();
    internal_.reset();
    return ret;
  }
  
 private:
  std::weak_ptr<TcpInternal> internal_;
};

class TcpManager {
 public:
  TcpManager() = default;
  TcpManager(const TcpManager &) = delete;
  TcpManager(TcpManager &&) = delete;
  
  TcpManager &operator=(const TcpManager &) = delete;
  TcpManager &operator=(TcpManager &&) = delete;
  
  TcpSocket NewSocket() {
    std::unique_lock<std::mutex> lock(mtx_);
    auto result = tcp_internals_.emplace(
        next_tcp_id_, std::make_shared<TcpInternal>(next_tcp_id_, this, 0, 0));
    ++next_tcp_id_;
    lock.unlock();
    
    assert(result.second == true);
    return TcpSocket(result.first->second);
  }
  
  TcpSocket NewSocket(uint64_t id) {
    std::unique_lock<std::mutex> lock(mtx_);
    auto search_result = tcp_internals_.find(id);
    lock.unlock();
    
    assert(search_result != tcp_internals_.end());
    return TcpSocket(search_result->second);
  }
  
  TcpSocket AcceptConnection(TcpInternal *from) {
    uint64_t id = from->Id();
    auto search_result = incomming_connections_.find(id);
    assert(search_result != incomming_connections_.end());
    
    if (!search_result->second.empty()) {
      auto new_id = search_result->second.front();
      search_result->second.pop_front();
      return NewSocket(new_id);
    }
    return {};
  }
  
  int CloseInternal(uint64_t id) {
    assert(tcp_internals_.find(id) != tcp_internals_.end());
    // if listen
      // realease all waiting connection
    return 0;
  }
  
  void DeMultiplexing(std::shared_ptr<NetworkPackage> package) {
    // Construct A TcpPackage
    TcpPackage tcp_package(package);
    
    // find the TcpInternal according to the TcpPackage header
    
    // move it to the internal
    
    // if result == NewConnection
      // Insert a new connection to both tcp_internals
    
    // else if result == ConnectionClosed
      // Remove connection
    
    // Ask for sending
    
    // Add to resend queue
  }
  
 private:
  std::map<uint64_t, std::shared_ptr<TcpInternal> > tcp_internals_;
  std::map<uint64_t, std::list<uint64_t> > incomming_connections_;
  
  std::map<std::chrono::time_point<std::chrono::steady_clock>,
           std::weak_ptr<TcpInternal> > resend_queue_;
  
  uint64_t next_tcp_id_ = 1;

  std::mutex mtx_;
};

#endif // _TCP_H_