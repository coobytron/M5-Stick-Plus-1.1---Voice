#pragma once
// Optional, explicitly enabled ESP-NOW TX for existing Trail Echo. Arduino-ESP32 3.x+.
// Does NOT send GPS, commands, or connect to a Wi-Fi access point.
// Switch off in SIGNAL_OS.ino to retain the exact pre-bridge radio behavior.
#include <Arduino.h>
#include "TrailBridgeWire.h"
#ifndef SIGNAL_OS_TRAIL_BRIDGE_ENABLE
#define SIGNAL_OS_TRAIL_BRIDGE_ENABLE 0
#endif
#if SIGNAL_OS_TRAIL_BRIDGE_ENABLE
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>
#endif

namespace trailbridge {
class Sender {
public:
  bool begin() {
#if SIGNAL_OS_TRAIL_BRIDGE_ENABLE
    if(active_)return true;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    if(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE)!=ESP_OK)return false;
    if(esp_now_init()!=ESP_OK)return false;
    esp_now_peer_info_t peer={};
    for(int i=0;i<6;++i)peer.peer_addr[i]=0xff; // Motion data only; public broadcast.
    peer.channel=6;peer.ifidx=WIFI_IF_STA;peer.encrypt=false;
    if(esp_now_add_peer(&peer)!=ESP_OK) {esp_now_deinit();return false;}
    active_=true;
    return true;
#else
    return false;
#endif
  }
  void end() {
#if SIGNAL_OS_TRAIL_BRIDGE_ENABLE
    replaying_=false;
    if(active_){esp_now_deinit();active_=false;WiFi.mode(WIFI_OFF);}
#endif
  }
  // Called exactly when a fresh ride starts, never on opening a saved score.
  void startRide(uint32_t nowMs) {
    lastBin_=0;nextSeq_=0;
#if SIGNAL_OS_TRAIL_BRIDGE_ENABLE
    rideId_=esp_random() ^ nowMs;
    if(!rideId_) rideId_=1;
#else
    (void)nowMs;
#endif
  }
  // One transmit at most per call; no blocking, retry loops or catch-up bursts.
  template<class CoreT> void poll(const CoreT& data, bool recording, uint32_t nowMs) {
#if SIGNAL_OS_TRAIL_BRIDGE_ENABLE
    if(!active_||!recording)return;
    if(data.count()<lastBin_)lastBin_=data.count();
    if(data.count()<=lastBin_)return;
    const uint16_t i=lastBin_++;
    const auto& bin=data.bin(i);
    Motion m={rideId_,nextSeq_++,uint16_t(i+1),bin.rmsMg,bin.peak10Mg,bin.impactCount,nowMs};
    uint8_t payload[kPacketSize];encode(m,payload);
    static const uint8_t broadcast[6]={255,255,255,255,255,255};
    if(esp_now_send(broadcast,payload,kPacketSize)==ESP_OK)submitted_++;
    else rejected_++;
#else
    (void)data;(void)recording;(void)nowMs;
#endif
  }
  // Resend a ride that's already saved (B hold on the score screen), so a
  // Cardputer that missed it live can still capture it. Two passes so a
  // packet lost on the first usually arrives on the second. Paced, one
  // packet per poll and at most one every 6 ms; never while recording.
  template<class CoreT> bool startReplay(const CoreT& data, bool recording, uint32_t nowMs) {
#if SIGNAL_OS_TRAIL_BRIDGE_ENABLE
    if(!active_||recording||data.count()==0)return false;
    replayTotal_=data.count();replayNext_=0;replayPass_=0;replayLastMs_=nowMs;
    replaySendId_=esp_random()^nowMs;if(!replaySendId_)replaySendId_=1;
    replaying_=true;
    return true;
#else
    (void)data;(void)recording;(void)nowMs;
    return false;
#endif
  }
  void stopReplay(){replaying_=false;}
  template<class CoreT> void pollReplay(const CoreT& data, uint32_t nowMs) {
#if SIGNAL_OS_TRAIL_BRIDGE_ENABLE
    if(!replaying_)return;
    if(!active_||data.count()!=replayTotal_){replaying_=false;return;}
    if(uint32_t(nowMs-replayLastMs_)<6)return;
    replayLastMs_=nowMs;
    const auto& bin=data.bin(replayNext_);
    // rideId is only known for a ride recorded since this power-on.
    Replay r={rideId_,replayNext_,replayTotal_,bin.rmsMg,bin.peak10Mg,bin.impactCount,replaySendId_};
    uint8_t payload[kPacketSize];encodeReplay(r,payload);
    static const uint8_t broadcast[6]={255,255,255,255,255,255};
    if(esp_now_send(broadcast,payload,kPacketSize)!=ESP_OK){rejected_++;return;} // retry same bin next poll
    submitted_++;
    if(++replayNext_>=replayTotal_){replayNext_=0;if(++replayPass_>=2)replaying_=false;}
#else
    (void)data;(void)nowMs;
#endif
  }
  bool replaying()const{return replaying_;}
  // 0..100 across both passes.
  uint8_t replayPercent()const{
    if(!replayTotal_)return 0;
    return uint8_t((uint32_t(replayPass_)*replayTotal_+replayNext_)*100u/(2u*replayTotal_));
  }
  bool active()const{return active_;}
  uint32_t submitted()const{return submitted_;}
  uint32_t rejected()const{return rejected_;}
private:
  bool active_=false, replaying_=false;
  uint16_t lastBin_=0, nextSeq_=0, replayNext_=0, replayTotal_=0;
  uint8_t replayPass_=0;
  uint32_t replaySendId_=0, replayLastMs_=0;
  uint32_t rideId_=0,submitted_=0,rejected_=0;
};
} // namespace trailbridge
