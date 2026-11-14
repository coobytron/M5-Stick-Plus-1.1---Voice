#pragma once
// Protocol shared by original M5StickC Plus and Cardputer ADV. No Arduino dependencies.
// Packets carry motion summaries only: no GPS coordinates, MACs, identities or commands.
#include <stdint.h>
#include <stddef.h>

namespace trailbridge {
static const size_t kPacketSize = 22;
static const uint8_t kVersion = 1;
static const uint8_t kMotion = 1;
static const uint8_t kReplay = 2;  // a saved ride resent after it was recorded
static const uint16_t kMaxReplayBins = 7200;  // Trail Echo's 2-hour cap

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

// Replay: the Stick resends a ride it already saved, one packet per 1-second
// bin. Same 22-byte frame as Motion, kind 2, so older receivers just drop it.
//   4-7 rideId (0 = unknown, e.g. the Stick was restarted since the ride)
//   8-9 bin index from 0, 10-11 total bins, 12-13 rmsMg, 14 peak10Mg,
//   15 impacts, 16-19 sendId (random per send, groups one transfer)
struct Replay {
  uint32_t rideId;
  uint16_t index;
  uint16_t total;
  uint16_t rmsMg;
  uint8_t peak10Mg;
  uint8_t impacts;
  uint32_t sendId;
};
inline void encodeReplay(const Replay& r, uint8_t out[kPacketSize]) {
  out[0]='T';out[1]='E';out[2]=kVersion;out[3]=kReplay;
  put32(out+4,r.rideId);put16(out+8,r.index);put16(out+10,r.total);
  put16(out+12,r.rmsMg);out[14]=r.peak10Mg;out[15]=r.impacts;
  put32(out+16,r.sendId);put16(out+20,crc16(out,20));
}
inline bool decodeReplay(const uint8_t* in, size_t len, Replay& out) {
  if(!in||len!=kPacketSize||in[0]!='T'||in[1]!='E'||in[2]!=kVersion||in[3]!=kReplay) return false;
  if(get16(in+20)!=crc16(in,20)) return false;
  Replay r;
  r.rideId=get32(in+4);r.index=get16(in+8);r.total=get16(in+10);
  r.rmsMg=get16(in+12);r.peak10Mg=in[14];r.impacts=in[15];r.sendId=get32(in+16);
  if(r.sendId==0||r.total==0||r.total>kMaxReplayBins||r.index>=r.total) return false;
  out=r;return true;
}

// Receiver side of a replay: collects one transfer's bins in RAM (about
// 30 KB, so only instantiate it on the Cardputer). Repeats are ignored;
// bins that never arrive stay missing and are reported, never filled in.
class ReplayCollector {
 public:
  ReplayCollector() { clear(); }
  void clear() {
    active_=false;rideId_=0;sendId_=0;total_=0;have_=0;
    for(size_t i=0;i<sizeof(seen_);++i) seen_[i]=0;
  }
  // Returns true when the packet belongs to a different transfer than the one
  // being collected: save the current one first, then call clear() and add().
  bool startsNewTransfer(const Replay& r) const { return active_ && r.sendId!=sendId_; }
  bool add(const Replay& r) {
    if(!active_){active_=true;rideId_=r.rideId;sendId_=r.sendId;total_=r.total;}
    if(r.sendId!=sendId_||r.total!=total_||r.index>=total_) return false;
    if(seen_[r.index>>3]&(1u<<(r.index&7))) return false;
    seen_[r.index>>3]|=uint8_t(1u<<(r.index&7));
    rms_[r.index]=r.rmsMg;peak_[r.index]=r.peak10Mg;impacts_[r.index]=r.impacts;
    ++have_;return true;
  }
  bool active()const{return active_;}
  bool complete()const{return active_&&have_==total_;}
  uint32_t rideId()const{return rideId_;}
  uint32_t sendId()const{return sendId_;}
  uint16_t total()const{return total_;}
  uint16_t received()const{return have_;}
  uint16_t missing()const{return uint16_t(total_-have_);}
  bool has(uint16_t i)const{return i<total_&&(seen_[i>>3]&(1u<<(i&7)));}
  uint16_t rmsMg(uint16_t i)const{return rms_[i];}
  uint8_t peak10Mg(uint16_t i)const{return peak_[i];}
  uint8_t impacts(uint16_t i)const{return impacts_[i];}
 private:
  bool active_;uint32_t rideId_,sendId_;uint16_t total_,have_;
  uint8_t seen_[(kMaxReplayBins+7)/8];
  uint16_t rms_[kMaxReplayBins];
  uint8_t peak_[kMaxReplayBins];
  uint8_t impacts_[kMaxReplayBins];
};
} // namespace trailbridge
