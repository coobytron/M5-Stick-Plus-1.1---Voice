/*
 SIGNAL OS v3.1 + TRAIL ECHO / M5StickC Plus 1.x (135 x 240 portrait)
 BLE Radar / Device List / Inspector / History / Lost Alert
 Wi-Fi Radar / Network List / Invisible Weather
 Audio Scope / USB Voice Recorder / Trail Echo IMU Recorder

 Arduino libraries: M5Unified, M5GFX, NimBLE-Arduino 2.5.1
 Espressif Arduino-ESP32 core (same settings as Precision Sonar v2).

 Press A: open item / change BLE or Wi-Fi view
 Press B: next item / per-view action
 Hold A: home

 NOTE: Device angles are deterministic artistic placements, NOT bearings.
 Wi-Fi scans show access points, NOT network traffic or occupancy.
 USB recorder requires included record_on_mac.py running on Mac.
*/
#include <Arduino.h>
#include <M5Unified.h>
#include <M5GFX.h>
#include "SignalOS_TrailEcho.h"
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

constexpr int WIDTH=135, HEIGHT=240;
constexpr int MAX_BLE=32, MAX_WIFI=24;
constexpr int BLE_ROWS=7, WIFI_ROWS=8;
constexpr uint32_t BLE_KEEP_MS=17000;
constexpr uint32_t FRAME_MS=70;
constexpr uint32_t AUDIO_RATE=16000;
constexpr uint8_t HOME_ITEMS=6;
constexpr uint8_t STANDARD_BRIGHTNESS=88, TRAIL_REC_BRIGHTNESS=16;
constexpr int AUDIO_BLOCK=512;
constexpr float PI2=6.28318530718f;

enum View : uint8_t { HOME, BLE_RADAR, BLE_LIST, BLE_INSPECT, BLE_HISTORY,
 BLE_ALERT, WIFI_RADAR, WIFI_LIST, WEATHER, SCOPE, RECORDER, TRAIL_ECHO };
static View view=HOME;
static uint8_t homeSelection=0, bleSelection=0, wifiPage=0;
static bool namedOnly=false, showHidden=true, alarmEnabled=false, weatherDense=false;
static uint32_t lastFrame=0, lastHistory=0, lastWifiPoll=0, lastAlarm=0;
static char watchAddress[18]={};
static int16_t history[62]={};
static int historyHead=0;
static int historyCount=0;

struct BleRecord { char addr[18]; char name[25]; int16_t rssi; uint32_t seen, id; bool used; };
static BleRecord bleSlots[MAX_BLE]={}, bleList[MAX_BLE]={};
static int bleCount=0;
static portMUX_TYPE bleMux=portMUX_INITIALIZER_UNLOCKED;
static volatile bool bleScanning=false;
static bool bleInit=false;

struct WifiRecord { char ssid[31]; int16_t rssi; uint8_t channel; uint32_t id; };
static WifiRecord wifiList[MAX_WIFI]={};
static int wifiCount=0;
static bool wifiScanning=false;
static uint32_t lastWifiStart=0, lastWifiResult=0;
static int channelStrength[14]={};

static uint16_t BG, PANEL, RULE, RING, MUTED, UI_WHITE, LIME, AMBER, UI_ORANGE, UI_RED;
static M5Canvas canvas(&M5.Display);
static trailecho::Mode trailEcho;
static bool trailImuReady=false;

// Audio is transmitted as binary packets (SREC, D, E) at 921600 baud.
// Two pending mic buffers, callback->queue, main loop->serial, requeue.
static QueueHandle_t audioDone=nullptr;
static int16_t audioBuffers[2][AUDIO_BLOCK];
static bool recording=false, hostReady=false, micReady=false;
static uint32_t recordedBytes=0, startedAt=0, lastHello=0;
static int soundLevel=0;
static int16_t scopeSamples[115]={};
static char hostCommand[16]={};
static uint8_t commandLength=0;

uint32_t hashText(const char* s) {
  uint32_t h=2166136261UL;
  while (*s) { h^=(uint8_t)*s++; h*=16777619UL; }
  return h;
}
uint16_t rgb(int r,int g,int b) { return M5.Display.color565(r,g,b); }
uint16_t dim(uint16_t c, uint8_t strength) {
  return rgb(((((c>>11)&31)*255/31)*strength)/255,
             (((((c>>5)&63)*255/63)*strength)/255),
             ((((c&31)*255/31)*strength)/255));
}
uint16_t signalColor(int rssi) { return rssi>=-65?LIME:(rssi>=-80?AMBER:UI_ORANGE); }
String safeName(const char* name,int maxChars) {
  String s(name && name[0]?name:"UNKNOWN");
  for(size_t i=0;i<s.length();++i) if((uint8_t)s[i]<32 || (uint8_t)s[i]>126) s.setCharAt(i,'?');
  if((int)s.length()>maxChars) s=s.substring(0,maxChars-1)+".";
  return s;
}
void txt(int x,int y,const String& s,uint16_t c,int scale=1) {
  canvas.setTextSize(scale); canvas.setTextColor(c); canvas.drawString(s,x,y);
}
void rightTxt(int x,int y,const String& s,uint16_t c,int scale=1) {
  canvas.setTextSize(scale); canvas.setTextColor(c); canvas.drawRightString(s,x,y);
}
const char* viewName(View v) {
  switch(v) {
    case BLE_RADAR:return "BLE / RADAR"; case BLE_LIST:return "BLE / DEVICES";
    case BLE_INSPECT:return "BLE / INSPECT"; case BLE_HISTORY:return "BLE / HISTORY";
    case BLE_ALERT:return "BLE / ALERT"; case WIFI_RADAR:return "WIFI / RADAR";
    case WIFI_LIST:return "WIFI / NETWORKS"; case WEATHER:return "INVISIBLE WEATHER";
    case SCOPE:return "AUDIO / SCOPE"; case RECORDER:return "AUDIO / RECORD";
    case TRAIL_ECHO:return "TRAIL / ECHO";
    default:return "INSTRUMENTS";
  }
}

// --- BLE scan callback; short writes under spinlock ---
class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    std::string a=d->getAddress().toString(), n=d->getName();
    uint32_t now=millis(); int r=d->getRSSI();
    portENTER_CRITICAL(&bleMux);
    int slot=-1,freeSlot=-1,oldestSlot=0; uint32_t oldestAge=0;
    for(int i=0;i<MAX_BLE;++i) {
      if(bleSlots[i].used && strncmp(bleSlots[i].addr,a.c_str(),17)==0) {slot=i;break;}
      if(!bleSlots[i].used && freeSlot<0)freeSlot=i;
      if(bleSlots[i].used && now-bleSlots[i].seen>=oldestAge) {oldestAge=now-bleSlots[i].seen;oldestSlot=i;}
    }
    if(slot<0)slot=freeSlot>=0?freeSlot:oldestSlot;
    bool same=bleSlots[slot].used&&strncmp(bleSlots[slot].addr,a.c_str(),17)==0;
    if(!same)memset(&bleSlots[slot],0,sizeof(BleRecord));
    strncpy(bleSlots[slot].addr,a.c_str(),sizeof(bleSlots[slot].addr)-1);
    if(!n.empty())strncpy(bleSlots[slot].name,n.c_str(),sizeof(bleSlots[slot].name)-1);
    bleSlots[slot].rssi=same?(bleSlots[slot].rssi*3+r)/4:r;
    bleSlots[slot].seen=now;
    bleSlots[slot].id=hashText(bleSlots[slot].addr);
    bleSlots[slot].used=true;
    portEXIT_CRITICAL(&bleMux);
  }
  void onScanEnd(const NimBLEScanResults& results,int reason) override {
    if(bleScanning)NimBLEDevice::getScan()->start(12000,false,true);
  }
} scanCB;

bool isBle(View v) { return v>=BLE_RADAR && v<=BLE_ALERT; }
bool isWifi(View v) { return v>=WIFI_RADAR && v<=WEATHER; }
bool isAudio(View v) { return v==SCOPE || v==RECORDER; }
void startBle() {
  if(!bleInit) {
    NimBLEDevice::init("SIGNAL-OS");
    NimBLEScan* s=NimBLEDevice::getScan();
    s->setScanCallbacks(&scanCB,true);
    s->setActiveScan(true); s->setInterval(100); s->setWindow(70);
    s->setMaxResults(0);
    bleInit=true;
  }
  bleScanning=true;
  NimBLEDevice::getScan()->start(12000,false,true);
}
void stopBle() {
  if(!bleInit)return;
  bleScanning=false;
  NimBLEDevice::getScan()->stop();
}
void refreshBle() {
  BleRecord snapshot[MAX_BLE];
  portENTER_CRITICAL(&bleMux);
  memcpy(snapshot,bleSlots,sizeof(snapshot));
  portEXIT_CRITICAL(&bleMux);
  bleCount=0; uint32_t now=millis();
  for(int i=0;i<MAX_BLE;++i) {
    if(!snapshot[i].used || now-snapshot[i].seen>BLE_KEEP_MS)continue;
    if(namedOnly && !snapshot[i].name[0])continue;
    bleList[bleCount++]=snapshot[i];
  }
  for(int i=1;i<bleCount;++i) {
    BleRecord v=bleList[i];int j=i-1;
    while(j>=0 && bleList[j].rssi<v.rssi) {bleList[j+1]=bleList[j];--j;}
    bleList[j+1]=v;
  }
  if(bleSelection>=bleCount)bleSelection=bleCount?bleCount-1:0;
}
const BleRecord* selectedBle() {return bleCount?&bleList[bleSelection]:nullptr;}

void startWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false,false);
  WiFi.scanDelete();
  wifiScanning=false;
  lastWifiStart=0;
}
void stopWifi() {
  WiFi.scanDelete(); wifiScanning=false; WiFi.mode(WIFI_OFF);
}
void pollWifi() {
  uint32_t now=millis();
  if(wifiScanning) {
    int n=WiFi.scanComplete();
    if(n>=0) {
      wifiCount=0; memset(channelStrength,0,sizeof(channelStrength));
      for(int i=0;i<n;++i) {
        String name=WiFi.SSID(i); int rssi=WiFi.RSSI(i), channel=WiFi.channel(i);
        if(!showHidden && !name.length())continue;
        if(wifiCount<MAX_WIFI) {
          WifiRecord &v=wifiList[wifiCount++];memset(&v,0,sizeof(v));
          strncpy(v.ssid,name.c_str(),sizeof(v.ssid)-1);
          v.rssi=rssi; v.channel=(uint8_t)channel;
          String key=WiFi.BSSIDstr(i)+"/"+String(channel);
          v.id=hashText(key.c_str());
        }
        if(channel>=1&&channel<=13)channelStrength[channel]+=max(0,rssi+100);
      }
      for(int i=1;i<wifiCount;++i) {
        WifiRecord v=wifiList[i];int j=i-1;
        while(j>=0 && wifiList[j].rssi<v.rssi) {wifiList[j+1]=wifiList[j];--j;}
        wifiList[j+1]=v;
      }
      WiFi.scanDelete();wifiScanning=false;lastWifiResult=now;
    } else if(n==-2) {wifiScanning=false;}
  }
  if(!wifiScanning && (lastWifiStart==0 || now-lastWifiStart>9000)) {
    WiFi.scanNetworks(true,true);
    wifiScanning=true;lastWifiStart=now;
  }
}

void audioCallback(void*,void* data,size_t length) {
  if(!audioDone)return;
  uint8_t idx=(data==audioBuffers[0])?0:1;
  xQueueSend(audioDone,&idx,0);
}
void audioMicOn() {
  M5.Speaker.end();
  if(!micReady) micReady=M5.Mic.begin();
}
void audioMicOff() {
  if(micReady) {
    M5.Mic.end();micReady=false;
  }
}
void streamBegin() {
  if(recording || !hostReady)return;
  audioMicOff();
  if(!audioDone)audioDone=xQueueCreate(6,sizeof(uint8_t));
  if(!audioDone)return;
  xQueueReset(audioDone);
  M5.Mic.setBufferReleaseCallback(nullptr,audioCallback);
  audioMicOn();
  if(!micReady)return;
  Serial.write((const uint8_t*)"SREC",4);
  uint32_t sampleRate=AUDIO_RATE;
  Serial.write((const uint8_t*)&sampleRate,4);
  recordedBytes=0;startedAt=millis();recording=true;
  M5.Mic.record(audioBuffers[0],AUDIO_BLOCK,AUDIO_RATE);
  M5.Mic.record(audioBuffers[1],AUDIO_BLOCK,AUDIO_RATE);
}
void streamPump() {
  if(!recording)return;
  uint8_t index=0;int budget=4;
  while(budget-- && xQueueReceive(audioDone,&index,0)==pdTRUE) {
    const uint8_t frame='D';uint16_t size=sizeof(audioBuffers[0]);
    Serial.write(&frame,1);
    Serial.write((const uint8_t*)&size,2);
    Serial.write((const uint8_t*)audioBuffers[index],size);
    recordedBytes+=size;
    long sum=0;
    for(int i=0;i<AUDIO_BLOCK;i+=8)sum+=abs((int32_t)audioBuffers[index][i]);
    soundLevel=(int)min(100L,sum/700);
    M5.Mic.record(audioBuffers[index],AUDIO_BLOCK,AUDIO_RATE);
  }
}
void streamStop() {
  if(!recording)return;
  // No longer submit more blocks; pending audio is dropped at stop.
  recording=false;
  audioMicOff();
  M5.Mic.setBufferReleaseCallback(nullptr,nullptr);
  const uint8_t end='E';
  Serial.write(&end,1);
  Serial.write((const uint8_t*)&recordedBytes,4);
  Serial.flush();
}
void pollHost() {
  while(Serial.available()) {
    char c=(char)Serial.read();
    if(c=='\n'||c=='\r') {
      hostCommand[commandLength]='\0';
      if(strcmp(hostCommand,"HELLO")==0) {hostReady=true;lastHello=millis();}
      commandLength=0;
    } else if(commandLength<sizeof(hostCommand)-1 && c>=32 && c<=126) {
      hostCommand[commandLength++]=c;
    } else {commandLength=0;}
  }
  if(millis()-lastHello>5000)hostReady=false;
}

void enter(View next) {
  if(next==view)return;
  View prev=view;
  if(recording && next!=RECORDER)streamStop();
  if(prev==TRAIL_ECHO && next!=TRAIL_ECHO) {
    trailEcho.exit(millis());
    M5.Display.setBrightness(STANDARD_BRIGHTNESS);
  }
  if(isBle(prev) && !isBle(next))stopBle();
  if(isWifi(prev) && !isWifi(next))stopWifi();
  if(isAudio(prev) && !isAudio(next))audioMicOff();
  view=next;
  if(isBle(next)&&!isBle(prev))startBle();
  if(isWifi(next)&&!isWifi(prev))startWifi();
  if(next==SCOPE)audioMicOn();
  if(next==TRAIL_ECHO)trailEcho.enter();
  lastFrame=0;
}

void header(const String& info) {
  txt(8,8,"SIGNAL",UI_WHITE,2);
  canvas.fillCircle(123,14,3,view==RECORDER&&recording?UI_RED:LIME);
  canvas.drawFastHLine(8,30,119,RULE);
  txt(8,39,viewName(view),MUTED);
  rightTxt(127,39,info,LIME);
}
void footer(const String& left,const String& right) {
  canvas.drawFastHLine(8,221,119,RULE);
  txt(8,228,left,MUTED);rightTxt(127,228,right,MUTED);
}
void dot(int x,int y,int size,uint16_t c,bool selected=false) {
  canvas.fillCircle(x,y,size+3,dim(c,32));
  canvas.fillCircle(x,y,size+1,dim(c,90));
  canvas.fillCircle(x,y,size,c);
  if(selected)canvas.drawCircle(x,y,size+5,dim(c,160));
}
void rings(int cx,int cy,int r) {
  for(int i=1;i<=3;++i)canvas.drawCircle(cx,cy,i*r/3,RING);
  canvas.drawFastVLine(cx,cy-r-2,2*r+4,RULE);
  canvas.drawFastHLine(cx-r-2,cy,2*r+4,RULE);
  for(int deg=0;deg<360;deg+=45) {
    float a=deg*PI2/360;
    canvas.drawLine(cx+(int)(cosf(a)*(r+2)),cy+(int)(sinf(a)*(r+2)),
                    cx+(int)(cosf(a)*(r+5)),cy+(int)(sinf(a)*(r+5)),RING);
  }
  float sweep=(millis()%6500)*(PI2/6500.0f)-PI2/4;
  for(int i=5;i>=0;--i) {
    float a=sweep-(i*0.055f);
    canvas.drawLine(cx,cy,cx+(int)(cosf(a)*r),cy+(int)(sinf(a)*r),
                    dim(LIME,i==0?175:50-i*7));
  }
  canvas.fillCircle(cx,cy,3,UI_WHITE);
}
void drawHome() {
  header("V3.1 / TE");
  const char* names[]={"BLUETOOTH","WI-FI RADAR","INVISIBLE WX","SOUND SCOPE","VOICE REC","TRAIL ECHO"};
  const char* subtitles[]={"BLE FIELD","2.4GHz NETWORKS","WIRELESS PATTERNS","MICROPHONE","USB TO MAC","TERRAIN / MOTION"};
  for(int i=0;i<HOME_ITEMS;++i) {
    int y=55+i*25;
    if(i==homeSelection) {canvas.fillRoundRect(6,y-2,123,24,3,PANEL);canvas.fillRect(6,y-2,2,24,LIME);}
    txt(13,y,String(i+1)+" "+names[i],i==homeSelection?LIME:UI_WHITE);
    txt(25,y+11,subtitles[i],MUTED);
  }
  txt(8,208,"HOLD A FROM ANY VIEW",MUTED);
  footer("A OPEN","B NEXT");
}
void drawBleRadar() {
  header(String(bleCount)+" LIVE");rings(67,115,54);
  const uint32_t now=millis();
  for(int i=bleCount-1;i>=0;--i) {
    const auto& d=bleList[i];float a=(d.id%3600)*PI2/3600;
    int radius=constrain((int)map(d.rssi,-100,-40,51,9),9,51);
    int x=67+(int)(cosf(a)*radius),y=115+(int)(sinf(a)*radius);
    uint8_t bright=now-d.seen<5000?255:(now-d.seen<11000?140:70);
    dot(x,y,constrain((int)map(d.rssi,-100,-40,2,4),2,4),dim(signalColor(d.rssi),bright),i==bleSelection);
  }
  canvas.drawFastHLine(8,185,119,RULE);
  txt(8,193,"PEAK RSSI",MUTED);
  rightTxt(127,193,bleCount?String(bleList[0].rssi)+"dBm":"SEARCH",UI_WHITE);
  txt(8,208,namedOnly?"FILTER: NAMED":"FILTER: ALL",MUTED);
  footer("A LIST","B FILTER");
}
void drawBleList() {
  header(String(bleCount)+" LIVE");txt(8,57,"DEVICE",MUTED);rightTxt(127,57,"RSSI",MUTED);
  if(!bleCount)txt(10,93,"SCANNING...",UI_WHITE);
  int top=(bleSelection/BLE_ROWS)*BLE_ROWS;
  for(int j=0;j<BLE_ROWS;++j) {
    int i=top+j;if(i>=bleCount)break;
    int y=76+j*20;
    if(i==bleSelection){canvas.fillRoundRect(7,y-3,121,18,3,PANEL);canvas.fillRect(7,y-3,2,18,LIME);}
    canvas.fillCircle(13,y+5,2,signalColor(bleList[i].rssi));
    txt(20,y,safeName(bleList[i].name,12),UI_WHITE);
    rightTxt(126,y,String(bleList[i].rssi),signalColor(bleList[i].rssi));
  }
  footer("A INSPECT","B SELECT");
}
void drawBleInspect() {
  const auto *d=selectedBle();header(d?"ACTIVE":"NO DATA");
  if(!d){txt(10,100,"NO BLE DEVICE",UI_WHITE);footer("A HISTORY","B NEXT");return;}
  txt(9,61,safeName(d->name,17),UI_WHITE,1);
  txt(9,81,"SIGNAL STRENGTH",MUTED);
  txt(9,99,String(d->rssi),UI_WHITE,3);txt(89,117,"dBm",MUTED);
  int width=constrain((int)map(d->rssi,-100,-35,0,115),0,115);
  canvas.fillRect(9,140,116,5,RING);canvas.fillRect(9,140,width,5,signalColor(d->rssi));
  txt(9,158,"LAST SEEN",MUTED);rightTxt(126,158,String((millis()-d->seen)/1000)+"s",UI_WHITE);
  txt(9,176,"ADDR",MUTED);txt(9,188,String(d->addr),MUTED);
  footer("A HISTORY","B NEXT");
}
void drawBleHistory() {
  const auto *d=selectedBle();header(d?String(d->rssi)+"dBm":"NO DATA");
  txt(8,61,d?safeName(d->name,18):"SELECT A DEVICE",UI_WHITE);
  for(int y=92;y<=186;y+=23)canvas.drawFastHLine(8,y,119,RING);
  for(int x=0;x<historyCount-1;++x) {
    int i=(historyHead-historyCount+x+62)%62,j=(i+1)%62;
    int y1=constrain((int)map(history[i],-100,-35,190,88),88,190);
    int y2=constrain((int)map(history[j],-100,-35,190,88),88,190);
    canvas.drawLine(8+x*2,y1,8+(x+1)*2,y2,LIME);
  }
  txt(9,204,"-100   RSSI / 36S  -35",MUTED);
  footer("A ALERT","B DEVICE");
}
void drawBleAlert() {
  const auto *d=selectedBle(); header(alarmEnabled?"ARMED":"OFF");
  txt(8,61,alarmEnabled?"WATCHING":"NOT ARMED",alarmEnabled?LIME:MUTED,2);
  String label="NO DEVICE";
  bool present=false;
  if(alarmEnabled && watchAddress[0]) {
    label=String(watchAddress+12);
    for(int i=0;i<bleCount;++i)if(strcmp(bleList[i].addr,watchAddress)==0) {present=true;label=safeName(bleList[i].name,16);break;}
  } else if(d) label=safeName(d->name,16);
  txt(9,110,label,UI_WHITE);
  if(alarmEnabled){
    txt(9,137,present?"SIGNAL PRESENT":"SIGNAL MISSING",present?LIME:UI_ORANGE);
    txt(9,156,"ALERT AFTER 12 SEC",MUTED);
  } else txt(9,137,"B: ARM SELECTED",MUTED);
  footer("A RADAR","B ARM / OFF");
}
void drawWifiRadar() {
  header(String(wifiCount)+" AP");rings(67,115,54);
  for(int i=wifiCount-1;i>=0;--i) {
    const auto& d=wifiList[i];float a=(d.id%3600)*PI2/3600;
    int r=constrain((int)map(d.rssi,-100,-35,51,9),9,51);
    dot(67+(int)(cosf(a)*r),115+(int)(sinf(a)*r),3,signalColor(d.rssi),i==0);
  }
  canvas.drawFastHLine(8,186,119,RULE);
  txt(8,195,"STRONGEST",MUTED);
  rightTxt(127,195,wifiCount?String(wifiList[0].rssi)+" dBm":"SCANNING",UI_WHITE);
  txt(8,208,"2.4 GHz ONLY",MUTED);
  footer("A NETWORKS","B HIDDEN");
}
void drawWifiList() {
  header(String(wifiCount)+" AP");
  txt(9,58,"SSID",MUTED);rightTxt(126,58,"CH/RSSI",MUTED);
  if(!wifiCount)txt(8,94,"SCANNING...",UI_WHITE);
  int pages=max(1,(wifiCount+WIFI_ROWS-1)/WIFI_ROWS);wifiPage%=pages;
  for(int row=0;row<WIFI_ROWS;++row) {
    int i=wifiPage*WIFI_ROWS+row;if(i>=wifiCount)break;
    int y=75+row*17;
    txt(8,y,safeName(wifiList[i].ssid[0]?wifiList[i].ssid:"[HIDDEN]",12),UI_WHITE);
    rightTxt(127,y,String(wifiList[i].channel)+"/"+String(wifiList[i].rssi),signalColor(wifiList[i].rssi));
    canvas.drawFastHLine(8,y+14,119,dim(RULE,170));
  }
  footer("A RADAR","B PAGE");
}
void drawWeather() {
  header(String(wifiCount)+" AP");
  txt(8,59,"RADIO FIELD",UI_WHITE,2);
  txt(8,80,"2.4 GHz / AMBIENT",MUTED);
  uint32_t t=millis();
  // Not traffic monitoring: animated signal-field metaphor from observed AP RSSI.
  for(int j=0;j<16;++j){
    int x=7+j*8;
    int ch=1+j%11;
    float strength=constrain(channelStrength[ch]/70.0f,0.0f,4.0f);
    for(int y=102;y<188;y+=5) {
      float wiggle=sinf((float)y*.06f + j*.53f + t*.0011f*(weatherDense?2.4f:1.0f));
      int xx=x+(int)(wiggle*(2.5f+strength));
      int yy=y+(int)(cosf(t*.0007f+j*.4f)*3);
      uint16_t c=dim((j%4==0)?AMBER:LIME,(uint8_t)(65+min(135.0f,strength*27+y%31)));
      canvas.drawPixel(xx,yy,c);
    }
  }
  canvas.drawFastHLine(8,194,119,RULE);
  txt(8,203,weatherDense?"FIELD / ENERGIZED":"FIELD / CALM",MUTED);
  footer("A HOME (HOLD)","B STYLE");
}
void drawScope() {
  header(micReady?"MIC ON":"MIC OFF");
  txt(8,60,"SOUND FIELD",UI_WHITE,2);
  canvas.drawFastHLine(8,137,119,RING);
  for(int i=0;i<114;++i) {
    int y1=constrain(137-(scopeSamples[i]>>9),88,181);
    int y2=constrain(137-(scopeSamples[i+1]>>9),88,181);
    canvas.drawLine(9+i,y1,10+i,y2,i%9==0?UI_WHITE:LIME);
  }
  canvas.drawFastHLine(8,183,119,RULE);
  txt(8,195,"INPUT LEVEL",MUTED);
  rightTxt(127,195,String(soundLevel)+"%",UI_WHITE);
  footer("A RECORDER","B GAIN");
}
void drawRecorder() {
  header(recording?"REC":"USB");
  txt(8,61,"VOICE NOTES",UI_WHITE,2);
  if(recording) {
    canvas.fillCircle(22,104,8,UI_RED);
    txt(37,94,"RECORDING",UI_RED);
    uint32_t seconds=(millis()-startedAt)/1000;
    txt(8,127,String(seconds/60)+":"+String((seconds%60)/10)+String(seconds%10),UI_WHITE,3);
  } else {
    canvas.drawCircle(22,105,8,hostReady?LIME:MUTED);
    txt(37,98,hostReady?"HOST READY":"NO RECEIVER",hostReady?LIME:UI_ORANGE);
    txt(8,139,hostReady?"PRESS B TO RECORD":"RUN MAC SCRIPT",MUTED);
  }
  for(int i=0;i<19;++i) {
    int h=recording?constrain(3+soundLevel/6+(int)(5*fabsf(sinf(i*.7f+millis()*.003f))),3,24):3;
    canvas.fillRect(10+i*6,180-h,3,h,recording?(i%5==0?UI_WHITE:LIME):RING);
  }
  canvas.drawFastHLine(8,185,119,RULE);
  txt(8,195,"16K / MONO / PCM",MUTED);
  rightTxt(127,208,recording?String(recordedBytes/1024)+" KB":"USB-C",UI_WHITE);
  footer("HOLD A HOME",recording?"B STOP":"B START");
}
// Trail Echo UI is drawn into the SAME canvas as the other instruments.
// Read and post-ride analysis only: no screen interaction needed while cycling.
void drawTrailEcho() {
  const auto &data=trailEcho.data();
  header(trailEcho.recording()?"REC":data.count()?"SAVED":"READY");
  if(!trailImuReady) {
    txt(8,71,"IMU UNAVAILABLE",UI_RED,1);
    txt(8,98,"NO SENSOR FOUND",MUTED);
    txt(8,124,"CHECK BOARD",MUTED);
    footer("HOLD A HOME","NO RECORD");
    return;
  }
  if(trailEcho.recording()) {
    txt(8,63,"TERRAIN MEMORY",MUTED);
    canvas.drawCircle(67,128,39,RING);
    canvas.drawCircle(67,128,30,RULE);
    canvas.fillCircle(67,128,6,UI_ORANGE);
    txt(37,102,"CAPTURING",LIME);
    int seconds=data.count();
    txt(32,143,String(seconds/60)+":"+(seconds%60<10?"0":"")+String(seconds%60),UI_WHITE,2);
    txt(8,191,"50 HZ / IMU",MUTED);
    rightTxt(127,191,String(data.liveMg())+" mg",UI_WHITE);
    txt(8,205,"DISPLAY DIMMED",MUTED);
    footer("A STOP","HOLD A HOME");
  } else if(!data.count()) {
    txt(8,65,"TERRAIN MEMORY",UI_WHITE,2);
    txt(8,94,"TURN MOTION INTO",MUTED);
    txt(8,106,"A VISUAL SCORE.",MUTED);
    canvas.drawFastHLine(8,141,119,RING);
    for(int x=10;x<127;x+=6) {
      int h=(int)(fabsf(sinf(x*.23f))*13);
      canvas.drawFastVLine(x,141-h,2*h+1,(x%3==0)?LIME:RING);
    }
    txt(8,177,"A: BEGIN CAPTURE",LIME);
    txt(8,196,trailEcho.stored()?"FLASH STORAGE READY":"RAM ONLY - NO SAVE",trailEcho.stored()?MUTED:UI_ORANGE);
    footer("A RECORD","HOLD A HOME");
  } else if(trailEcho.page()==1) {
    txt(8,63,"RIDE / METRICS",UI_WHITE,1);
    txt(8,86,"DURATION",MUTED);
    rightTxt(126,84,String(data.count()/60)+":"+(data.count()%60<10?"0":"")+String(data.count()%60),UI_WHITE,2);
    canvas.drawFastHLine(8,111,119,RULE);
    txt(8,122,"AVG RMS",MUTED);
    rightTxt(126,122,String(data.averageRmsMg())+" mg",LIME);
    txt(8,144,"PEAK RMS",MUTED);
    rightTxt(126,144,String(data.maxRmsMg())+" mg",UI_WHITE);
    txt(8,166,"IMPACTS",MUTED);
    rightTxt(126,166,String(data.totalImpacts()),UI_ORANGE);
    if(trailEcho.sensorErrors())txt(8,189,String(trailEcho.sensorErrors())+" READ ERR",UI_ORANGE);
    else txt(8,189,"RELATIVE VIBRATION",MUTED);
    txt(8,204,trailEcho.stored()?"SAVED IN FLASH":"NOT SAVED / RAM ONLY",trailEcho.stored()?MUTED:UI_ORANGE);
    footer("A NEW","B SCORE");
  } else {
    txt(8,59,"TERRAIN SIGNATURE",UI_WHITE);
    rightTxt(127,72,String(data.count())+" s",MUTED);
    const int left=8, span=119, top=88, bottom=186;
    for(int y=bottom;y>=top;y-=24)canvas.drawFastHLine(left,y,span,RING);
    // Auto-scale to the captured ride, preserving peak-per-pixel events.
    const uint16_t fullScale=max((int)350,(int)data.maxRmsMg());
    for(int x=0;x<span;++x) {
      uint32_t first=(uint32_t(x)*data.count())/span;
      uint32_t after=(uint32_t(x+1)*data.count())/span;
      if(after<=first)after=first+1;
      if(after>data.count())after=data.count();
      uint16_t intensity=0; bool shock=false;
      for(uint32_t i=first;i<after;++i) {
        const auto &b=data.bin((uint16_t)i);
        if(b.rmsMg>intensity)intensity=b.rmsMg;
        if(b.impactCount)shock=true;
      }
      int h=constrain((int)((uint32_t)intensity*(bottom-top)/fullScale),0,bottom-top);
      if(h>0) canvas.drawFastVLine(left+x,bottom-h,h,shock?UI_ORANGE:LIME);
      if(shock) canvas.drawPixel(left+x,bottom-h,UI_WHITE);
    }
    txt(8,197,"TIME  /  RMS ACCEL",MUTED);
    rightTxt(127,197,String(fullScale)+"mg",MUTED);
    if(!trailEcho.stored())txt(8,209,"RAM ONLY",UI_ORANGE);
    footer("A NEW","B STATS");
  }
}

void draw() {
  canvas.fillSprite(BG);
  switch(view){
    case HOME:drawHome();break;
    case BLE_RADAR:drawBleRadar();break;
    case BLE_LIST:drawBleList();break;
    case BLE_INSPECT:drawBleInspect();break;
    case BLE_HISTORY:drawBleHistory();break;
    case BLE_ALERT:drawBleAlert();break;
    case WIFI_RADAR:drawWifiRadar();break;
    case WIFI_LIST:drawWifiList();break;
    case WEATHER:drawWeather();break;
    case SCOPE:drawScope();break;
    case RECORDER:drawRecorder();break;
    case TRAIL_ECHO:drawTrailEcho();break;
  }
  canvas.pushSprite(0,0);
}

void onA() {
  if(view==HOME) {
    static const View choices[]={BLE_RADAR,WIFI_RADAR,WEATHER,SCOPE,RECORDER,TRAIL_ECHO};
    enter(choices[homeSelection]);
  } else if(isBle(view)) {
    enter(view==BLE_ALERT?BLE_RADAR:(View)((int)view+1));
  } else if(view==WIFI_RADAR)enter(WIFI_LIST);
  else if(view==WIFI_LIST)enter(WIFI_RADAR);
  else if(view==SCOPE)enter(RECORDER);
  else if(view==RECORDER)enter(SCOPE);
  else if(view==TRAIL_ECHO && trailImuReady) {
    trailEcho.shortA(millis());
    M5.Display.setBrightness(trailEcho.recording()?TRAIL_REC_BRIGHTNESS:STANDARD_BRIGHTNESS);
  }
}
void onB() {
  if(view==HOME)homeSelection=(homeSelection+1)%HOME_ITEMS;
  else if(view==BLE_RADAR)namedOnly=!namedOnly;
  else if(view==BLE_LIST || view==BLE_INSPECT || view==BLE_HISTORY) {
    if(bleCount)bleSelection=(bleSelection+1)%bleCount;
    historyCount=0;historyHead=0;
  } else if(view==BLE_ALERT) {
    alarmEnabled=!alarmEnabled;
    const auto* d=selectedBle();
    if(alarmEnabled && d) {strncpy(watchAddress,d->addr,17);watchAddress[17]=0;lastAlarm=millis();}
    else alarmEnabled=false;
  } else if(view==WIFI_RADAR){showHidden=!showHidden;}
  else if(view==WIFI_LIST)wifiPage++;
  else if(view==WEATHER)weatherDense=!weatherDense;
  else if(view==SCOPE)soundLevel=0;
  else if(view==RECORDER){if(recording)streamStop();else streamBegin();}
  else if(view==TRAIL_ECHO)trailEcho.shortB();
}
void pollAudioScope() {
  if(view!=SCOPE || !micReady)return;
  static int16_t buff[256];
  if(!M5.Mic.record(buff,256,AUDIO_RATE))return;
  uint32_t start=millis();
  while(M5.Mic.isRecording() && millis()-start<30)delay(1);
  long sum=0;for(int i=0;i<256;i+=4)sum+=abs((int32_t)buff[i]);
  int val=(int)min(100L,sum/650);
  soundLevel=(soundLevel*3+val)/4;
  for(int i=0;i<115;++i)scopeSamples[i]=buff[i*2];
}
void tickHistory() {
  if(!isBle(view)||millis()-lastHistory<600)return;
  lastHistory=millis();
  const auto*d=selectedBle();
  history[historyHead]=d?d->rssi:-100;
  historyHead=(historyHead+1)%62;
  historyCount=min(62,historyCount+1);
}
void checkAlarm() {
  if(!alarmEnabled || !watchAddress[0] || !isBle(view))return;
  bool found=false;
  for(int i=0;i<bleCount;++i)if(strcmp(watchAddress,bleList[i].addr)==0){found=true;break;}
  if(found){lastAlarm=millis();return;}
  if(millis()-lastAlarm>12000) {
    static uint32_t lastBeep=0;
    if(millis()-lastBeep>1500) {
      lastBeep=millis();
      // M5StickC Plus buzzer support depends on installed M5Unified release.
      if(M5.Speaker.isEnabled())M5.Speaker.tone(2600,120);
    }
  }
}
// M5Unified must update its IMU before reading current acceleration (in g).
static bool readTrailAccel(float& x,float& y,float& z) {
  if(!trailImuReady) return false;
  if(!M5.Imu.update()) return false;
  return M5.Imu.getAccel(&x,&y,&z);
}

void setup() {
  auto cfg=M5.config();M5.begin(cfg);
  trailImuReady=M5.Imu.isEnabled();
  trailEcho.begin(); // Never formats flash: RAM fallback if SPIFFS is unavailable.

  M5.Display.setRotation(0); M5.Display.setBrightness(STANDARD_BRIGHTNESS);
  Serial.begin(921600); // host receiver must use identical baud
  BG=rgb(8,13,17);PANEL=rgb(24,41,34);RULE=rgb(43,57,58);
  RING=rgb(48,67,65);MUTED=rgb(125,145,147);UI_WHITE=rgb(238,244,239);
  LIME=rgb(173,255,112);AMBER=rgb(238,193,104);
  UI_ORANGE=rgb(218,115,82);UI_RED=rgb(244,93,79);
  canvas.setColorDepth(16);
  if(!canvas.createSprite(WIDTH,HEIGHT)){
    M5.Display.fillScreen(BG);M5.Display.setTextColor(UI_RED);
    M5.Display.drawString("SPRITE MEMORY FAIL",5,100);
    while(true)delay(1000);
  }
  draw();
}
void loop() {
  M5.update();pollHost();
  if(view==TRAIL_ECHO) {
    const bool wasRecording=trailEcho.recording();
    trailEcho.update(millis(),readTrailAccel);
    if(wasRecording && !trailEcho.recording())M5.Display.setBrightness(STANDARD_BRIGHTNESS);
  }
  if(recording)streamPump();
  if(M5.BtnA.wasHold())enter(HOME);
  else if(M5.BtnA.wasClicked())onA();
  if(M5.BtnB.wasClicked())onB();
  uint32_t now=millis();
  if(isWifi(view) && now-lastWifiPoll>150){lastWifiPoll=now;pollWifi();}
  if(isBle(view)) {refreshBle();tickHistory();checkAlarm();}
  if(view==SCOPE && now-lastFrame>=FRAME_MS)pollAudioScope();
  if(now-lastFrame>=FRAME_MS) {lastFrame=now;draw();}
  delay(3);
}
