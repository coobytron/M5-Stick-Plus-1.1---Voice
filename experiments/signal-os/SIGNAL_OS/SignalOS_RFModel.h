#pragma once
// Pure-C++ RSSI model for RF 424 experiments; no radio dependency.
#include <stdint.h>

namespace signalrf {
static const uint8_t kBands=9;
static const int16_t kMissing=-127;
inline uint32_t centerKHz(uint8_t band) {return 433200u + uint32_t(band%kBands)*200u;}
inline uint8_t strength(int16_t dbm) {
  if(dbm<=-110)return 0;
  if(dbm>=-55)return 100;
  return uint8_t((int(dbm)+110)*100/55);
}
class QuietWatch {
 public:
  void arm(bool enabled, uint32_t now) {
    armed_=enabled; seen_=false; lastActivity_=now;
  }
  void feed(int16_t dbm,uint32_t now) {
    if(!armed_)return;
    if(dbm>=threshold_) {seen_=true;lastActivity_=now;}
  }
  bool armed() const {return armed_;}
  bool seen() const {return seen_;}
  bool quiet(uint32_t now)const {
    return armed_ && seen_ && uint32_t(now-lastActivity_)>=12000u;
  }
  uint32_t age(uint32_t now)const {
    return seen_?uint32_t(now-lastActivity_):0u;
  }
  void setThreshold(int16_t dbm){threshold_=dbm;}
  int16_t threshold() const {return threshold_;}
 private:
  bool armed_=false,seen_=false;
  uint32_t lastActivity_=0;
  int16_t threshold_=-75;
};
} // namespace signalrf
