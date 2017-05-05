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

class NetworkPacket {
 public:
  static auto NewPacket(size_t size) {
    return std::shared_ptr<NetworkPacket>(new NetworkPacket(size));
  }
  
  static auto NewPacket(const char *begin, const char *end) {
    assert(begin <= end);
    auto packet = NewPacket(static_cast<size_t>(end - begin));
    std::copy(begin, end, packet->begin_);
    return packet;
  }
  
  std::pair<char *, char *> GetBuffer() {
    return {begin_, end_};
  }
  
  size_t Length() const {
    return static_cast<size_t>(end_ - begin_);
  }
  
  ~NetworkPacket() {
    delete[] begin_;
  }
  
 private:
  NetworkPacket() = default;
  
  NetworkPacket(size_t size) : begin_(new char[size]), end_(begin_+size) {}
  
  NetworkPacket(const NetworkPacket &) = delete;
  NetworkPacket(NetworkPacket &&) = delete;

  NetworkPacket &operator=(const NetworkPacket &) = delete;
  NetworkPacket &operator=(NetworkPacket &&) = delete;
  
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

// NetworkPacket wrapper
class TcpPacket {
 public:
  // Consturct an empty TcpPacket
  TcpPacket() = default;
  
  // Adapt a NetworkPacket
  explicit TcpPacket(std::shared_ptr<NetworkPacket> packet)
      : network_packet_(packet) {
    if (packet->Length() < sizeof(TcpHeader))
      throw std::runtime_error("TcpPacket length error");
  }
      
  // Consturct a TcpPacket from a copy of a TcpHeader and a range of memory
  TcpPacket(const TcpHeader &header, const char *first, const char *last) {
    network_packet_ = NetworkPacket::NewPacket(
      static_cast<size_t>(last-first) + sizeof(TcpHeader));
    std::copy(reinterpret_cast<const char *>(&header), 
              reinterpret_cast<const char *>(&header+1),
              network_packet_->GetBuffer().first);
    std::copy(first, last,
              network_packet_->GetBuffer().first+sizeof(TcpHeader));
  }
  
  // Consturct a TcpPacket from a copy of a range of memory and default header
  TcpPacket(const char *first, const char *last) {
    network_packet_ = NetworkPacket::NewPacket(
      static_cast<size_t>(last-first) + sizeof(TcpHeader));
    new(network_packet_->GetBuffer().first) TcpHeader();
    std::copy(first, last,
              network_packet_->GetBuffer().first+sizeof(TcpHeader));
  }
  
  TcpPacket(const TcpPacket &) = delete;
  TcpPacket(TcpPacket &&) = default;
  
  TcpPacket &operator=(const TcpPacket &) = delete;
  TcpPacket &operator=(TcpPacket &&) = default;
  
  std::pair<char *, char *> GetData() {
    char *begin, *end;
    std::tie(begin, end) = network_packet_->GetBuffer();
    return {begin + sizeof(TcpHeader), end};
  }
  
  explicit operator std::shared_ptr<NetworkPacket>() {
    return network_packet_;
  }
  
  TcpHeader &GetHeader() {
    return const_cast<TcpHeader &>(
        static_cast<const TcpPacket *>(this)->GetHeader());
  }
  const TcpHeader &GetHeader() const {
    return reinterpret_cast<const TcpHeader &>(
        *network_packet_->GetBuffer().first);
  }
  
  auto TotalLength() const {
    return network_packet_->Length();
  }
  
  size_t Length() const {
    assert(TotalLength() >= sizeof(TcpHeader));
    return TotalLength() - sizeof(TcpHeader);
  }
  
  bool ValidByChecksum() {
    return CalculateChecksum() == 0;
  }
  
  uint16_t CalculateChecksum();
  
  std::weak_ptr<NetworkPacket> WeakFromThis() {
    return GetNetworkPacket();
  }
  
 private:
  std::shared_ptr<NetworkPacket> GetNetworkPacket() {
    return network_packet_;
  }
  
  std::shared_ptr<NetworkPacket> network_packet_;
};

class TcpBuffer {
 public:
  TcpPacket *GetFrontWritePacket() {
    if (write_buffer_.empty())
      return nullptr;
    return &write_buffer_.front();
  }
  
  void MoveFrontWriteToUnack() {
    if (!write_buffer_.empty()) {
      unack_packets_.emplace_back(std::move(write_buffer_.front()));
      write_buffer_.pop_front();
    }
  }
  
  void PopWiriteBuffer() {
    write_buffer_.pop_front();
  }
  
  auto GetReadPackets() {
    std::list<TcpPacket> packets(
        std::make_move_iterator(read_buffer_.begin()),
        std::make_move_iterator(read_buffer_.end()));
    read_buffer_.clear();
    return packets;
  }
  
  // obsolete
  // template <class UnaryFunction>
  // auto GetPacketsForResending(UnaryFunction fn) {
  //   std::list<std::shared_ptr<NetworkPacket> > packets;
  //   std::transform(unack_packets_.begin(), unack_packets_.end(),
  //                  std::back_inserter(packets),
  //                  [&fn](auto &x){
  //                    fn(x);
  //                    return static_cast<std::shared_ptr<NetworkPacket> >(x);
  //                  });
  //   read_buffer_.clear();
  //   return packets;
  // }
  
  void AddToReadBuffer(TcpPacket ptr) {
    std::cerr << __func__ << std::endl;
    read_buffer_.push_back(std::move(ptr));
  }
  
  void AddToWriteBuffer(TcpPacket ptr) {
    std::cerr << __func__ << std::endl;
    write_buffer_.push_back(std::move(ptr));
  }
  
  int Ack(uint32_t acknowledge_number) {
    std::cerr << __func__  << " Buffer Called - ";
    while (!unack_packets_.empty() &&
           acknowledge_number >=
              unack_packets_.front().GetHeader().SequenceNumber() + 
                  unack_packets_.front().Length()) {
      std::cerr << __func__ << " ";
      unack_packets_.pop_front();
    }
    std::cerr << unack_packets_.size() << std::endl;
    return 0;
  }
  
  void Clear() {
    read_buffer_.clear();
    write_buffer_.clear();
    unack_packets_.clear();
  }
  
 private:
  std::list<TcpPacket> read_buffer_;
  std::list<TcpPacket> write_buffer_;
  
  std::list<TcpPacket> unack_packets_;
};

std::ostream &operator<<(std::ostream &o,
                         const std::shared_ptr<NetworkPacket> &ptr);

std::ostream &operator<<(std::ostream &o,
                         const TcpHeader &packet);

std::ostream &operator<<(std::ostream &o,
                         const TcpPacket &packet);



} // namespace tcp_simulator

#endif // _TCP_BUFFER_H_
