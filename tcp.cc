#include "tcp.h"

std::list<std::shared_ptr<NetworkPackage> >
TcpInternal::GetPackagesForSending() {
  
  std::list<std::shared_ptr<NetworkPackage> > result;
  for (;;) {
    auto package = buffer_.GetFrontWritePackage();
    if (package == nullptr)
      break;
    
    std::cerr << __func__ << " @ ";
    std::cerr << "GetSeq" << package->Length() << " ";
    auto sequence_number = state_.GetSequenceNumber(package->Length());
    if (sequence_number.second == false)
      break;
    package->GetHeader().SequenceNumber() = sequence_number.first;
    
    std::cerr << "Psh ";
    std::cerr << std::endl;
    state_.PrepareHeader(package->GetHeader(), package->Length());
    package->GetHeader().Checksum() = 0;
    package->GetHeader().Checksum() = package->CalculateChecksum();
    result.emplace_back(static_cast<std::shared_ptr<NetworkPackage> >(
        *package));
    buffer_.MoveFrontWriteToUnack();
  }
  
  return result;
}

std::list<std::shared_ptr<NetworkPackage> >
TcpInternal::GetPackagesForResending() {
  return buffer_.GetPackagesForResending(
      [this](TcpPackage &package) {
        state_.PrepareResendHeader(package.GetHeader(), 0);
        package.GetHeader().Checksum() = 0;
        package.GetHeader().Checksum() = package.CalculateChecksum();
      });
}
  
int TcpInternal::AddPackageForSending(TcpPackage package) {
  package.GetHeader().SourcePort() = host_port_;
  package.GetHeader().DestinationPort() = peer_port_;
  buffer_.AddToWriteBuffer(std::move(package));
  return 0;
}

int TcpInternal::AddPackageForSending(const char *begin, const char *end) {
  return AddPackageForSending(TcpPackage(begin, end));
}

void TcpInternal::ReceivePackage(TcpPackage package) {
  if (package.ValidByChecksum() == false) {
    std::cerr << "Checksum failed" << std::endl;
    Discard();
    return ;
  }
  current_package_ = std::move(package);
  auto size = current_package_.Length();
  
  state_.OnReceivePackage(&current_package_.GetHeader(), size);
  while (!unsequenced_packages_.empty()) {
    auto ite = unsequenced_packages_.begin();
    auto &package = ite->second;
    if (!state_.OnSequencePackage(package.GetHeader(), package.Length()))
      break;
    buffer_.AddToReadBuffer(std::move(package));
    unsequenced_packages_.erase(ite);
  }
  
  std::cerr << "#" << id_ << state_;
}

int TcpInternal::CloseInternal() {
  return manager_->CloseInternal(id_);
}

TcpSocket TcpInternal::AcceptConnection() {
  assert(manager_);

  return manager_->AcceptConnection(this);
}

void TcpInternal::Reset() {
  std::cerr << __func__ << std::endl;
  state_.SetState(0, State::kClosed, Stage::kClosed);
  unsequenced_packages_.clear();
  buffer_.Clear();
  host_port_ = peer_port_ = 0;
  
  CloseInternal();
}

void TcpInternal::NewConnection() {
  std::cerr << __func__ << std::endl;
  manager_->NewConnection(id_, std::move(current_package_));
}

void TcpInternal::Connect(uint16_t port, uint32_t seq, uint16_t window) {
  peer_port_ = port;
  assert(host_port_ != 0 && peer_port_ != 0);
  
  manager_->RegestBound(host_port_, peer_port_, id_);
  state_.SendSyn(seq, window);
}

int TcpInternal::Listen(uint16_t port) {
  assert(host_port_ != 0);
  auto result = state_.Listen();
  assert(result);
  
  host_port_ = port;
  peer_port_ = 0;
  manager_->RegestBound(host_port_, peer_port_, id_);
  return 0;
}

TcpSocket TcpSocket::Accept() {
  return internal_.lock()->AcceptConnection();
}

void TcpSocket::Connect(uint16_t port) {
  internal_.lock()->Connect(port, 1425, 10240);
}
