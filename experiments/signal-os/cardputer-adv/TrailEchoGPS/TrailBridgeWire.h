#pragma once
// Protocol shared by original M5StickC Plus and Cardputer ADV. No Arduino dependencies.
// Packets carry motion summaries only: no GPS coordinates, MACs, identities or commands.
#include <stdint.h>
#include <stddef.h>

namespace trailbridge {
static const size_t kPacketSize = 22;
static const uint8_t kVersion = 1;
static const uint8_t kMotion = 1;

struct Motion {
  uint32_t rideId;
  uint16_t sequence;
  uint16_t elapsedSeconds;
  uint16_t rmsMg;
  uint8_t peak10Mg;
  uint8_t impacts;
  uint32_t senderMs;
};

inline void put16(uint8_t* p, uint16_t n) { p[0] = uint8_t(n); p[1] = uint8_t(n >> 8); }
inline void put32(uint8_t* p, uint32_t n) { for(int i=0;i<4;++i) p[i]=uint8_t(n>>(i*8)); }
inline uint16_t get16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline uint32_t get32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24); }
inline uint16_t crc16(const uint8_t* data, size_t n) {
  uint16_t crc = 0xffff;
  for(size_t i=0;i<n;++i) {
    crc ^= uint16_t(data[i]) << 8;
    for(uint8_t b=0;b<8;++b) crc = (crc & 0x8000) ? uint16_t((crc<<1)^0x1021) : uint16_t(crc<<1);
  }
  return crc;
}
inline void encode(const Motion& m, uint8_t out[kPacketSize]) {
  out[0]='T';out[1]='E';out[2]=kVersion;out[3]=kMotion;
  put32(out+4,m.rideId);put16(out+8,m.sequence);put16(out+10,m.elapsedSeconds);
  put16(out+12,m.rmsMg);out[14]=m.peak10Mg;out[15]=m.impacts;
  put32(out+16,m.senderMs);put16(out+20,crc16(out,20));
}
inline bool decode(const uint8_t* in, size_t len, Motion& out) {
  if(!in||len!=kPacketSize||in[0]!='T'||in[1]!='E'||in[2]!=kVersion||in[3]!=kMotion) return false;
  if(get16(in+20)!=crc16(in,20)) return false;
  Motion m;
  m.rideId=get32(in+4);m.sequence=get16(in+8);m.elapsedSeconds=get16(in+10);
  m.rmsMg=get16(in+12);m.peak10Mg=in[14];m.impacts=in[15];m.senderMs=get32(in+16);
  if(m.rideId==0||m.elapsedSeconds==0) return false;
  out=m;return true;
}
// A missing packet stays missing; this tracker only labels duplicates/reordering.
class Receiver {
 public:
  Receiver():known_(false),ride_(0),seq_(0),accepted_(0),duplicates_(0),gaps_(0){}
  bool accept(const Motion& m) {
    if (!known_||m.rideId!=ride_) {known_=true;ride_=m.rideId;seq_=m.sequence;accepted_++;return true;}
    uint16_t d=uint16_t(m.sequence-seq_);
    if(d==0||d>=0x8000){duplicates_++;return false;}
    if(d>1)gaps_+=uint32_t(d-1);
    seq_=m.sequence;accepted_++;return true;
  }
  uint32_t accepted()const{return accepted_;}
  uint32_t duplicates()const{return duplicates_;}
  uint32_t gaps()const{return gaps_;}
 private:
  bool known_;uint32_t ride_;uint16_t seq_;uint32_t accepted_,duplicates_,gaps_;
};
} // namespace trailbridge
