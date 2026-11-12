#pragma once
// Receive-only CC1101 RF424 sampler for original M5StickC Plus 1.x.
// Build without the RadioLib dependency by leaving RF feature flag = 0.
#include "SignalOS_RFModel.h"
#ifndef SIGNAL_OS_RF424_CC1101
#define SIGNAL_OS_RF424_CC1101 0
#endif
#if SIGNAL_OS_RF424_CC1101
#include <SPI.h>
#include <RadioLib.h>
// RadioLib 8.0 changed FSK frequency arguments from MHz floats to Hz integers.
// v7.8.1 is the supported, pinned driver for this opt-in experiment.
#if RADIOLIB_VERSION_MAJOR >= 8
#error "SIGNAL OS RF424: install RadioLib 7.8.1 in Arduino Library Manager (8.x API is incompatible)"
#endif
#endif

namespace signalrf {
class Sampler {
 public:
  bool ready()const{return ready_;}
  bool hasData()const{return samples_>0;}
  uint8_t band()const{return lastBand_;}
  int16_t lastRssi()const{return lastRssi_;}
  int16_t at(uint8_t i)const{return bins_[i%kBands];}
  uint32_t sampleCount()const{return samples_;}
  int16_t peak()const{
    int16_t v=kMissing;
    for(uint8_t i=0;i<kBands;i++)if(bins_[i]>v)v=bins_[i];
    return v;
  }
  void ensure(uint32_t now) {
#if SIGNAL_OS_RF424_CC1101
    if(ready_ || (attempted_ && now-lastAttempt_<4000))return;
    attempted_=true;lastAttempt_=now;
    rfSpi().begin(0,33,32,26);
    const int16_t rc=radio().begin(433.2f);
    ready_=(rc==RADIOLIB_ERR_NONE && radio().receiveDirectAsync()==RADIOLIB_ERR_NONE);
    lastStep_=now;lastBand_=0;
#else
    (void)now;
#endif
  }
  // One tuned-frequency sample every ~125 ms: this is activity/signal energy,
  // not decoded packets, source identity, exact spectrum occupancy, or range.
  bool poll(uint32_t now) {
#if SIGNAL_OS_RF424_CC1101
    if(!ready_)return false;
    if(uint32_t(now-lastStep_)<125u)return false;
    const float dBm=radio().getRSSI(); // receiveDirectAsync() gives live RSSI
    int measured=int(dBm);
    if(measured<-127 || measured>0)return false;
    bins_[lastBand_]=int16_t(measured);
    lastRssi_=int16_t(measured);
    if(samples_<0xffffffffu)++samples_;
    const uint8_t recorded=lastBand_;
    lastBand_=uint8_t((lastBand_+1u)%kBands);
    radio().standby();
    const float nextMHz=float(centerKHz(lastBand_))/1000.0f;
    if(radio().setFrequency(nextMHz)!=RADIOLIB_ERR_NONE ||
       radio().receiveDirectAsync()!=RADIOLIB_ERR_NONE)ready_=false;
    lastStep_=now;
    reportedBand_=recorded;
    return true;
#else
    (void)now;return false;
#endif
  }
  uint8_t reportedBand()const{return reportedBand_;}
 private:
#if SIGNAL_OS_RF424_CC1101
  static SPIClass& rfSpi(){static SPIClass spi(HSPI);return spi;}
  static CC1101& radio(){
    // Bruce original-Plus "legacy" pin mapping. Validate on physical RF424.
    static CC1101 cc(new Module(26,25,RADIOLIB_NC,RADIOLIB_NC,
                               rfSpi(),SPISettings(2000000,MSBFIRST,SPI_MODE0)));
    return cc;
  }
#endif
  bool ready_=false,attempted_=false;
  uint32_t lastAttempt_=0,lastStep_=0,samples_=0;
  uint8_t lastBand_=0,reportedBand_=0;
  int16_t lastRssi_=kMissing;
  int16_t bins_[kBands]={kMissing,kMissing,kMissing,kMissing,kMissing,kMissing,kMissing,kMissing,kMissing};
};
} // namespace signalrf
