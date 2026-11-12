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
  bool active()const{return active_;}
  uint32_t submitted()const{return submitted_;}
  uint32_t rejected()const{return rejected_;}
private:
  bool active_=false;
  uint16_t lastBin_=0, nextSeq_=0;
  uint32_t rideId_=0,submitted_=0,rejected_=0;
};
} // namespace trailbridge
