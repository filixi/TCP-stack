#ifndef _TCP_BUFFER_H_
#define _TCP_BUFFER_H_

#include <cstdint>
#include <cassert>

#include <algorithm>
#include <array>
#include <iostream>
#include <list>
#include <memory>
#include <stdexcept>
#include <utility>

namespace tcp_simulator {

class NetworkPackage {
 public:
  static auto NewPackage(size_t size) {
    return std::shared_ptr<NetworkPackage>(new NetworkPackage(size));
  }
  
  static auto NewPackage(const char *begin, const char *end) {
    assert(begin <= end);
    auto package = NewPackage(static_cast<size_t>(end - begin));
    std::copy(begin, end, package->begin_);
    return package;
  }
  
  std::pair<char *, char *> GetBuffer() {
    return {begin_, end_};
  }
  
  size_t Length() const {
    return static_cast<size_t>(end_ - begin_);
  }
  
  ~NetworkPackage() {
    delete[] begin_;
  }
  
 private:
  NetworkPackage() = default;
  
  NetworkPackage(size_t size) : begin_(new char[size]), end_(begin_+size) {}
  
  NetworkPackage(const NetworkPackage &) = delete;
  NetworkPackage(NetworkPackage &&) = delete;

  NetworkPackage &operator=(const NetworkPackage &) = delete;
  NetworkPackage &operator=(NetworkPackage &&) = delete;
  
  char *begin_ = nullptr;
  char *end_ = nullptr;
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
  Field<5> field_;
};

// only adapt a NetworkPackage
class TcpPackage {
 public:
  // Consturct an empty TcpPackage
  TcpPackage() = default;
  
  // Adapt a NetworkPackage
  explicit TcpPackage(std::shared_ptr<NetworkPackage> package)
      : network_package_(package) {
    if (package->Length() < sizeof(TcpHeader))
      throw std::runtime_error("TcpPackage length error");
  }
      
  // Consturct a TcpPackage from a copy of a TcpHeader and a range of memory
  TcpPackage(const TcpHeader &header, const char *first, const char *last) {
    network_package_ = NetworkPackage::NewPackage(
      static_cast<size_t>(last-first) + sizeof(TcpHeader));
    std::copy(reinterpret_cast<const char *>(&header), 
              reinterpret_cast<const char *>(&header+1),
              network_package_->GetBuffer().first);
    std::copy(first, last,
              network_package_->GetBuffer().first+sizeof(TcpHeader));
  }
  
  // Consturct a TcpPackage from a copy of a range of memory and default header
  TcpPackage(const char *first, const char *last) {
    network_package_ = NetworkPackage::NewPackage(
      static_cast<size_t>(last-first) + sizeof(TcpHeader));
    new(network_package_->GetBuffer().first) TcpHeader();
    std::copy(first, last,
              network_package_->GetBuffer().first+sizeof(TcpHeader));
  }
  
  TcpPackage(const TcpPackage &) = delete;
  TcpPackage(TcpPackage &&) = default;
  
  TcpPackage &operator=(const TcpPackage &) = delete;
  TcpPackage &operator=(TcpPackage &&) = default;
  
  std::pair<char *, char *> GetData() {
    char *begin, *end;
    std::tie(begin, end) = network_package_->GetBuffer();
    return {begin + sizeof(TcpHeader), end};
  }
  
  explicit operator std::shared_ptr<NetworkPackage>() {
    return network_package_;
  }
  
  TcpHeader &GetHeader() {
    return const_cast<TcpHeader &>(
        static_cast<const TcpPackage *>(this)->GetHeader());
  }
  const TcpHeader &GetHeader() const {
    return reinterpret_cast<const TcpHeader &>(
        *network_package_->GetBuffer().first);
  }
  
  auto TotalLength() const {
    return network_package_->Length();
  }
  
  size_t Length() const {
    assert(TotalLength() >= sizeof(TcpHeader));
    return TotalLength() - sizeof(TcpHeader);
  }
  
  bool ValidByChecksum() {
    return CalculateChecksum() == 0;
  }
  
  uint16_t CalculateChecksum();
  
 private:
  auto GetNetworkPackage() {
    return network_package_;
  }
  
  std::shared_ptr<NetworkPackage> network_package_;
};

class TcpBuffer {
 public:
  TcpPackage *GetFrontWritePackage() {
    if (write_buffer_.empty())
      return nullptr;
    return &write_buffer_.front();
  }
  
  void MoveFrontWriteToUnack() {
    if (!write_buffer_.empty()) {
      unack_packages_.emplace_back(std::move(write_buffer_.front()));
      write_buffer_.pop_front();
    }
  }
  
  auto GetReadPackages() {
    std::list<TcpPackage> packages(
        std::make_move_iterator(read_buffer_.begin()),
        std::make_move_iterator(read_buffer_.end()));
    read_buffer_.clear();
    return packages;
  }
  
  template <class UnaryFunction>
  auto GetPackagesForResending(UnaryFunction fn) {
    std::list<std::shared_ptr<NetworkPackage> > packages;
    std::transform(unack_packages_.begin(), unack_packages_.end(),
                   std::back_inserter(packages),
                   [&fn](auto &x){
                     fn(x);
                     return static_cast<std::shared_ptr<NetworkPackage> >(x);
                   });
    read_buffer_.clear();
    return packages;
  }
  
  void AddToReadBuffer(TcpPackage ptr) {
    std::cerr << __func__ << std::endl;
    read_buffer_.push_back(std::move(ptr));
  }
  
  void AddToWriteBuffer(TcpPackage ptr) {
    std::cerr << __func__ << std::endl;
    write_buffer_.push_back(std::move(ptr));
  }
  
  int Ack(uint32_t acknowledge_number) {
    std::cerr << __func__  << " Buffer Called - ";
    while (!unack_packages_.empty() &&
           acknowledge_number >
              unack_packages_.front().GetHeader().SequenceNumber() + 
                  unack_packages_.front().Length()) {
      std::cerr << __func__ << " ";
      unack_packages_.pop_front();
    }
    std::cerr << std::endl;
    return 0;
  }
  
  void Clear() {
    read_buffer_.clear();
    write_buffer_.clear();
    unack_packages_.clear();
  }
  
 private:
  std::list<TcpPackage> read_buffer_;
  std::list<TcpPackage> write_buffer_;
  
  std::list<TcpPackage> unack_packages_;
};

std::ostream &operator<<(std::ostream &o,
                         const std::shared_ptr<NetworkPackage> &ptr);

std::ostream &operator<<(std::ostream &o,
                         const TcpPackage &package);

} // namespace tcp_simulator

#endif // _TCP_BUFFER_H_