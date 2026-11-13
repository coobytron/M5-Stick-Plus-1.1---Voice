#pragma once
// SIGNAL OS universal remote store + IR TX + capture of compatible fixed-code RF.
// Designed for M5StickC Plus 1.x (IR TX GPIO9; CC1101 GDO0 GPIO25).
// Local-only: no cloud, no transmit on boot, no filesystem formatting.
#include <Arduino.h>
#include <SPIFFS.h>
#include "SignalOS_RF424.h"

namespace signalremote {
static const uint8_t MAX_COMMANDS=16;
static const uint16_t MAX_PULSES=192;
static const uint8_t RF_RAW=1, IR_RAW=2;
static const uint8_t IR_TX_PIN=9, RF_DATA_PIN=25;
static const uint32_t RF_CENTER_KHZ=433920UL;

struct Command {
  char name[22];
  uint8_t kind;
  uint8_t firstHigh;
  uint32_t frequency; // IR carrier Hz (usually 38000), or RF carrier kHz (433920)
  uint16_t count;
  uint16_t pulses[MAX_PULSES]; // durations in microseconds, alternating levels
};
struct FileImage {
  uint32_t magic;
  uint16_t version;
  uint8_t count;
  uint8_t reserved;
  Command commands[MAX_COMMANDS];
};
static const uint32_t MAGIC=0x524D5331UL; // RMS1
static const uint16_t VERSION=1;
static const char* const STORAGE="/signal_remotes_v1.bin";

class Library {
public:
  void begin() {
    available_=SPIFFS.begin(false); // never erase stored Trail Echo rides
    if(!available_)return;
    File file=SPIFFS.open(STORAGE,"r");
    if(!file)return;
    FileImage* image=new(std::nothrow) FileImage;
    if(!image){file.close();return;}
    const size_t n=file.read(reinterpret_cast<uint8_t*>(image),sizeof(*image));
    file.close();
    if(n==sizeof(*image) && image->magic==MAGIC &&
       image->version==VERSION && image->count<=MAX_COMMANDS) {
      bool valid=true;
      for(uint8_t i=0;i<image->count;i++) {
        const Command& c=image->commands[i];
        if((c.kind!=RF_RAW && c.kind!=IR_RAW) || c.count<2 ||
           c.count>MAX_PULSES || !c.name[0] || c.name[21]!=0 ||
           (c.kind==IR_RAW && (c.frequency<30000 || c.frequency>60000)) ||
           (c.kind==RF_RAW && c.frequency!=RF_CENTER_KHZ)) {valid=false;break;}
        for(uint16_t j=0;j<c.count;j++)if(c.pulses[j]<65 || c.pulses[j]>15000){valid=false;break;}
      }
      if(valid) {
        count_=image->count;
        memcpy(commands_,image->commands,sizeof(commands_));
      }
    }
    delete image;
  }
  bool available()const{return available_;}
  uint8_t size()const{return count_;}
  const Command* at(uint8_t i)const{return i<count_?&commands_[i]:nullptr;}
  bool add(const Command& c) {
    if(count_>=MAX_COMMANDS || !valid(c))return false;
    commands_[count_++]=c;
    if(!save()){--count_;return false;}
    return true;
  }
  bool rename(uint8_t i,const char* name) {
    if(i>=count_ || !name || !*name)return false;
    char old[22];memcpy(old,commands_[i].name,22);
    cleanName(commands_[i].name,name);
    if(save())return true;
    memcpy(commands_[i].name,old,22);return false;
  }
  bool remove(uint8_t i) {
    if(i>=count_)return false;
    Command backup=commands_[i];
    for(uint8_t k=i;k+1<count_;++k)commands_[k]=commands_[k+1];
    --count_;
    if(save())return true;
    for(uint8_t k=count_;k>i;--k)commands_[k]=commands_[k-1];
    commands_[i]=backup;++count_;return false;
  }
  static void cleanName(char (&out)[22],const char* in) {
    uint8_t k=0;
    for(;in && *in && k<21;++in) {
      char c=*in;
      if(c>=32 && c<=126 && c!='|')out[k++]=c;
    }
    out[k]=0;
  }
  bool importLine(const String& line) {
    // IRRAW|label|38000|9000,4500,560,560,...  (Flipper raw timing upload)
    // RENAME|index|label  and DELETE|index (zero-based)
    if(line.startsWith("RENAME|")) {
      int p=line.indexOf('|',7);if(p<0)return false;
      return rename(uint8_t(line.substring(7,p).toInt()),line.substring(p+1).c_str());
    }
    if(line.startsWith("DELETE|"))return remove(uint8_t(line.substring(7).toInt()));
    if(!line.startsWith("IRRAW|"))return false;
    int p=line.indexOf('|',6),q=p<0?-1:line.indexOf('|',p+1);
    if(p<0||q<0||p<=6)return false;
    Command c={};
    c.kind=IR_RAW;c.firstHigh=1;
    cleanName(c.name,line.substring(6,p).c_str());
    c.frequency=uint32_t(line.substring(p+1,q).toInt());
    if(c.frequency<30000||c.frequency>60000)return false;
    int pos=q+1;
    while(pos<(int)line.length() && c.count<MAX_PULSES) {
      int next=line.indexOf(',',pos);
      if(next<0)next=line.length();
      String part=line.substring(pos,next);
      if(part.isEmpty())return false;
      long duration=part.toInt();
      if(duration<65||duration>15000)return false;
      c.pulses[c.count++]=uint16_t(duration);
      pos=next+1;
    }
    if(pos<(int)line.length())return false; // do not silently truncate
    return add(c);
  }
private:
  bool valid(const Command& c)const {
    if(!available_ || !c.name[0] || c.count<2 || c.count>MAX_PULSES)return false;
    if(c.kind==RF_RAW && c.frequency!=RF_CENTER_KHZ)return false;
    if(c.kind==IR_RAW && (c.frequency<30000||c.frequency>60000))return false;
    if(c.kind!=RF_RAW && c.kind!=IR_RAW)return false;
    for(uint16_t i=0;i<c.count;i++)if(c.pulses[i]<65||c.pulses[i]>15000)return false;
    return true;
  }
  bool save() {
    if(!available_)return false;
    FileImage* image=new(std::nothrow) FileImage;
    if(!image)return false;
    memset(image,0,sizeof(*image));
    image->magic=MAGIC;image->version=VERSION;image->count=count_;
    memcpy(image->commands,commands_,sizeof(commands_));
    // Use staging file so a failed write does not destroy the last good library.
    File f=SPIFFS.open("/signal_remotes_tmp.bin","w");
    if(!f){delete image;return false;}
    size_t written=f.write(reinterpret_cast<const uint8_t*>(image),sizeof(*image));
    f.close();delete image;
    if(written!=sizeof(FileImage)){SPIFFS.remove("/signal_remotes_tmp.bin");return false;}
    SPIFFS.remove(STORAGE);
    if(!SPIFFS.rename("/signal_remotes_tmp.bin",STORAGE))return false;
    return true;
  }
  bool available_=false;
  uint8_t count_=0;
  Command commands_[MAX_COMMANDS]={};
};

#if SIGNAL_OS_RF424_CC1101
static volatile bool edgeActive=false;
static volatile uint16_t edgeCount=0;
static volatile uint32_t lastEdgeMicros=0;
static volatile uint8_t firstEdgeLevel=0;
static volatile uint16_t edgePulses[MAX_PULSES]={};
static void IRAM_ATTR recordEdge() {
  if(!edgeActive)return;
  const uint32_t now=micros();
  const uint8_t current=uint8_t(digitalRead(RF_DATA_PIN)!=0);
  if(lastEdgeMicros==0 || uint32_t(now-lastEdgeMicros)>14000u) {
    edgeCount=0;firstEdgeLevel=current;lastEdgeMicros=now;return;
  }
  const uint32_t dt=uint32_t(now-lastEdgeMicros);
  if(dt>=65u && dt<=14000u && edgeCount<MAX_PULSES)edgePulses[edgeCount++]=uint16_t(dt);
  lastEdgeMicros=now;
}
#endif

class Engine {
public:
  void begin(){library.begin();}
  Library library;
  bool rfReady()const{return rfReady_;}
  bool capturing()const{return capturing_;}
  const char* status()const{return status_;}
  void open(signalrf::Sampler& rf){
    rfReady_=rf.startRemote();
    status_=rfReady_?"433.92 RX READY":"RF424 OFF / CHECK SWITCH";
  }
  void close(signalrf::Sampler& rf){
    stopCapture();rf.endRemote();rfReady_=false;
  }
  void startCapture() {
    if(!rfReady_){status_="RF424 NOT ACTIVE";return;}
#if SIGNAL_OS_RF424_CC1101
    stopCapture();
    noInterrupts();
    edgeCount=0;lastEdgeMicros=0;firstEdgeLevel=0;edgeActive=true;
    interrupts();
    pinMode(RF_DATA_PIN,INPUT);
    attachInterrupt(digitalPinToInterrupt(RF_DATA_PIN),recordEdge,CHANGE);
    capturing_=true;captureStart_=millis();status_="PRESS YOUR 433 REMOTE";
#else
    status_="ENABLE CC1101 FLAG";
#endif
  }
  void stopCapture() {
#if SIGNAL_OS_RF424_CC1101
    if(capturing_){
      detachInterrupt(digitalPinToInterrupt(RF_DATA_PIN));
      noInterrupts();edgeActive=false;interrupts();
    }
#endif
    capturing_=false;
  }
  void poll() {
#if SIGNAL_OS_RF424_CC1101
    if(!capturing_)return;
    const uint32_t now=micros();
    uint32_t last;uint16_t count;
    noInterrupts();last=lastEdgeMicros;count=edgeCount;interrupts();
    if(last && count>=20 && uint32_t(now-last)>35000UL){
      stopCapture();
      Command cmd={};cmd.kind=RF_RAW;cmd.frequency=RF_CENTER_KHZ;
      noInterrupts();
      cmd.firstHigh=firstEdgeLevel;
      cmd.count=edgeCount;
      if(cmd.count>MAX_PULSES)cmd.count=MAX_PULSES;
      for(uint16_t i=0;i<cmd.count;i++)cmd.pulses[i]=edgePulses[i];
      interrupts();
      char name[22];snprintf(name,sizeof(name),"RF %02u",unsigned(library.size()+1));
      Library::cleanName(cmd.name,name);
      if(library.add(cmd)){status_="RF CAPTURE SAVED";saved_=true;}
      else status_="SAVE FAILED / FULL";
    }else if(millis()-captureStart_>12000UL){
      stopCapture();status_="TIMEOUT / TRY AGAIN";
    }
#endif
  }
  bool takeSaved(){bool result=saved_;saved_=false;return result;}
  bool send(uint8_t index,signalrf::Sampler& rf) {
    const Command* cmd=library.at(index);
    if(!cmd)return false;
    if(cmd->kind==IR_RAW) {
      if(!ledcAttach(IR_TX_PIN,cmd->frequency,8)) {status_="IR PWM FAILED";return false;}
      ledcWrite(IR_TX_PIN,0);
      bool mark=cmd->firstHigh!=0;
      for(uint16_t i=0;i<cmd->count;i++){
        ledcWrite(IR_TX_PIN,mark?128:0);
        delayMicroseconds(cmd->pulses[i]);
        mark=!mark;
      }
      ledcWrite(IR_TX_PIN,0);
      ledcDetach(IR_TX_PIN);
      status_="IR TRANSMITTED";return true;
    }
#if SIGNAL_OS_RF424_CC1101
    if(!rfReady_){status_="RF424 NOT ACTIVE";return false;}
    stopCapture();
    const bool ok=rf.sendRemoteRaw(cmd->pulses,cmd->count,cmd->firstHigh!=0);
    status_=ok?"433 TX COMPLETE":"433 TX FAILED";
    return ok;
#else
    (void)rf;
    status_="ENABLE CC1101 FLAG";return false;
#endif
  }
private:
  bool rfReady_=false,capturing_=false,saved_=false;
  uint32_t captureStart_=0;
  const char* status_="READY";
};

} // namespace signalremote
