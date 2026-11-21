#pragma once
// SIGNAL OS: address-pinned BLE signal monitor. Pure C++11, no radio driver.
// RSSI is NOT measured distance, direction or an identity guarantee.
#include <stdint.h>
#include <string.h>
namespace bleprox {
constexpr int16_t kNoValue=-127;
constexpr uint8_t kHistory=60;
enum class State:uint8_t {NoTarget,Searching,Near,Mid,Far,Stale,Missing};
class Monitor {
public:
  void select(const char* addr,const char* name,uint32_t now) {
    address_[0]=0;name_[0]=0;have_=false;value_=kNoValue;
    lastSeen_=now;lastGraph_=now;head_=size_=0;threshold_=1;
    for(uint8_t i=0;i<kHistory;++i)graph_[i]=kNoValue;
    if(!addr||!addr[0])return;
    strncpy(address_,addr,sizeof(address_)-1);
    address_[sizeof(address_)-1]=0;
    if(name)strncpy(name_,name,sizeof(name_)-1);
    name_[sizeof(name_)-1]=0;
  }
  bool selected()const{return address_[0]!=0;}
  const char* address()const{return address_;}
  const char* name()const{return name_;}
  int16_t rssi()const{return value_;}
  uint32_t ageMs(uint32_t now)const{return have_?now-lastSeen_:0;}
  int16_t threshold()const{return threshold_==0?-55:threshold_==1?-65:-75;}
  void cycleThreshold(){threshold_=uint8_t((threshold_+1)%3);}
  bool observe(const char* addr,int16_t rssi,uint32_t seen) {
    if(!selected()||!addr||strcmp(addr,address_)!=0||
       rssi<-120||rssi>-15)return false;
    if(have_) {
      uint32_t delta=seen-lastSeen_;
      if(delta==0||delta>=0x80000000u)return false; // duplicate or older
      value_=int16_t((int32_t(value_)*2+rssi)/3);
    } else value_=rssi;
    have_=true;lastSeen_=seen;return true;
  }
  State state(uint32_t now)const{
    if(!selected())return State::NoTarget;
    if(!have_)return State::Searching;
    uint32_t age=now-lastSeen_;
    if(age>12000u)return State::Missing;
    if(age>3500u)return State::Stale;
    if(value_>=threshold())return State::Near;
    if(value_>=threshold()-12)return State::Mid;
    return State::Far;
  }
  bool fresh(uint32_t now)const{
    State s=state(now);
    return s==State::Near||s==State::Mid||s==State::Far;
  }
  void tick(uint32_t now){
    if(uint32_t(now-lastGraph_)<400u)return;
    lastGraph_=now;
    graph_[head_]=fresh(now)?value_:kNoValue; // gaps remain gaps
    head_=uint8_t((head_+1)%kHistory);
    if(size_<kHistory)++size_;
  }
  uint8_t points()const{return size_;}
  int16_t point(uint8_t chronological)const{
    if(chronological>=size_)return kNoValue;
    return graph_[(head_+kHistory-size_+chronological)%kHistory];
  }
private:
  char address_[18]={},name_[25]={};
  int16_t value_=kNoValue,graph_[kHistory]={};
  uint32_t lastSeen_=0,lastGraph_=0;
  uint8_t head_=0,size_=0,threshold_=1;
  bool have_=false;
};
}
