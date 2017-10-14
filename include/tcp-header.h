#ifndef _TCP_STATE_MACHINE_TCP_HEADER_H_
#define _TCP_STATE_MACHINE_TCP_HEADER_H_

#include <cassert>

#include <memory>
#include <ostream>

#include <iostream>

namespace tcp_stack {
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

private:
  Field<3> prefixed_field_;
  Field<5> field_;
};

class TcpPacket {
public:
  TcpPacket(TcpPacket &&) = default;
  TcpPacket &operator=(TcpPacket &&) = default;

  auto &GetHeader() {
    return reinterpret_cast<TcpHeader &>(*buff_.get());
  }

  auto &GetHeader() const {
    return reinterpret_cast<const TcpHeader &>(*buff_.get());
  }

  char *begin() {
    return buff_.get() + sizeof(TcpHeader);
  }

  const char *begin() const {
    return buff_.get() + sizeof(TcpHeader);
  }

  char *end() {
    return buff_.get() + size_;
  }

  const char *end() const {
    return buff_.get() + size_;
  }

  friend uint16_t CalculateChecksum(const TcpPacket &packet) {
    uint32_t checksum = 0;
    uint16_t * const buffer = reinterpret_cast<uint16_t *>(packet.buff_.get());
    const auto size = packet.size_;
    size_t i = 0;
    for (; i<size/2; ++i)
      checksum += buffer[i];
    if (size%2)
      checksum += buffer[size-1];

    return static_cast<uint16_t>(~checksum);
  }

protected:
  TcpPacket(size_t size)
      : size_(sizeof(TcpHeader) + size), buff_(new char[size_]) {
    new(buff_.get()) TcpHeader;
  }
  
  TcpPacket(const char *buff, size_t size)
      : size_(sizeof(TcpHeader) + size), buff_(new char[size_]) {
    new(buff_.get()) TcpHeader;
    std::copy(buff, buff+size, begin());
  }

private:
  TcpPacket(const TcpPacket &) = delete;
  TcpPacket &operator=(const TcpPacket &) = delete;

  size_t size_;
  std::unique_ptr<char []> buff_;
};

std::ostream &operator<<(std::ostream &o, const TcpHeader &header);

inline std::shared_ptr<TcpPacket> MakeTcpPacket(size_t size) {
  struct EnableMake : TcpPacket {
    EnableMake(size_t size) : TcpPacket(size) {}
  };
  return std::make_shared<EnableMake>(size);
}

inline std::shared_ptr<TcpPacket> MakeTcpPacket(
    const char *buff, size_t size) {
  struct EnableMake : TcpPacket {
    EnableMake(const char *buff, size_t size) : TcpPacket(buff, size) {}
  };
  return std::make_shared<EnableMake>(buff, size);
}

} // namespace tcp_stack

#endif // _TCP_STATE_MACHINE_TCP_HEADER_H_
