/*
 Trail Echo GPS companion / Cardputer ADV + Cap LoRa-1262.
 Independent Cardputer ADV sketch, NOT a SIGNAL OS replacement.
 Cap GNSS on UART: RX=GPIO15 (cap GPS_TX), TX=GPIO13 (cap GPS_RX), 115200 8N1.
 Cardputer ADV SD: SPI SCK40/MISO39/MOSI14/CS12.
 SX1262 never initialized or transmitted: GNSS works independently of LoRa.
 ESP-NOW ch6 receives Trail Echo motion packets (no location transmitted).
 Use only with LoRa antenna installed, per M5Stack hardware warning.
 Requires M5Cardputer, M5Unified, TinyGPSPlus, Arduino-ESP32 3.x.
*/
#include <Arduino.h>
#include <M5Cardputer.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <SPI.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "TrailBridgeWire.h"

static const int GPS_RX=15, GPS_TX=13, SD_CS=12;
static const int SD_SCK=40,SD_MISO=39,SD_MOSI=14;
static const int CHANNEL=6;
static HardwareSerial gpsUart(1);
static TinyGPSPlus gps;
static QueueHandle_t incoming=nullptr;
static trailbridge::Receiver peerTracker;
static File gpsLog, motionLog;
static bool sdReady=false, radioReady=false;
static char gpsPath[20]={}, motionPath[20]={};
static uint32_t lastLog=0,lastDraw=0,lastSync=0,invalidFrames=0, queueDrops=0,received=0;
static uint32_t lastPacketMs=0;
static uint32_t lastFlushMs=0;
struct RXMessage {uint8_t bytes[trailbridge::kPacketSize];uint8_t mac[6];uint32_t receivedMs;};

#if ESP_IDF_VERSION_MAJOR >= 5
void onNowReceive(const esp_now_recv_info_t *info,const uint8_t *data,int length) {
  if(!incoming||!info||!data||length!=(int)trailbridge::kPacketSize)return;
  RXMessage r={}; memcpy(r.bytes,data,trailbridge::kPacketSize);
  memcpy(r.mac,info->src_addr,6);r.receivedMs=millis();
  if(xQueueSend(incoming,&r,0)!=pdTRUE) ++queueDrops;
}
#else
void onNowReceive(const uint8_t *mac,const uint8_t *data,int length) {
  if(!incoming||!mac||!data||length!=(int)trailbridge::kPacketSize)return;
  RXMessage r={};memcpy(r.bytes,data,trailbridge::kPacketSize);
  memcpy(r.mac,mac,6);r.receivedMs=millis();
  if(xQueueSend(incoming,&r,0)!=pdTRUE) ++queueDrops;
}
#endif

bool openLogs(){
  // Cap LoRa and microSD share the SPI bus. Keep the unused SX1262 deselected.
  pinMode(5,OUTPUT);digitalWrite(5,HIGH);
  SPI.begin(SD_SCK,SD_MISO,SD_MOSI,SD_CS);
  pinMode(SD_CS,OUTPUT);digitalWrite(SD_CS,HIGH);
  if(!SD.begin(SD_CS,SPI,10000000))return false;
  for(int i=0;i<100;i++){
    snprintf(gpsPath,sizeof(gpsPath),"/G%02d.CSV",i);
    snprintf(motionPath,sizeof(motionPath),"/I%02d.CSV",i);
    if(SD.exists(gpsPath)||SD.exists(motionPath))continue;
    gpsLog=SD.open(gpsPath,FILE_WRITE);
    motionLog=SD.open(motionPath,FILE_WRITE);
    if(!gpsLog||!motionLog){if(gpsLog)gpsLog.close();if(motionLog)motionLog.close();return false;}
    gpsLog.println("local_ms,utc_iso,fix,lat,lon,hdop,satellites");
    motionLog.println("rx_ms,ride_id,seq,elapsed_s,rms_mg,peak10mg,impacts,tx_ms");
    gpsLog.flush();motionLog.flush();return true;
  }
  return false;
}

void initEspNow(){
  incoming=xQueueCreate(24,sizeof(RXMessage));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false,false);
  if(esp_wifi_set_channel(CHANNEL,WIFI_SECOND_CHAN_NONE)!=ESP_OK)return;
  if(esp_now_init()!=ESP_OK)return;
  if(esp_now_register_recv_cb(onNowReceive)!=ESP_OK){esp_now_deinit();return;}
  radioReady=true;
  Serial.printf("ESP-NOW ch=%d MAC=%s\n",CHANNEL,WiFi.macAddress().c_str());
}

String utcString(){
  if(!gps.date.isValid()||!gps.time.isValid()||gps.date.age()>5000||gps.time.age()>5000)return "";
  char s[32];snprintf(s,sizeof(s),"%04d-%02d-%02dT%02d:%02d:%02dZ",
    gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());
  return String(s);
}
void logFix(uint32_t now){
  const bool valid=gps.location.isValid()&&gps.location.age()<2500;
  const String utc=valid?utcString():String("");
  String line=String(now)+","+utc+","+(valid?"1":"0")+",";
  if(valid){line+=String(gps.location.lat(),7)+","+String(gps.location.lng(),7);}
  line+=",";
  if(valid&&gps.hdop.isValid()&&gps.hdop.age()<5000)line+=String(gps.hdop.hdop(),2);
  line+=",";
  if(gps.satellites.isValid()&&gps.satellites.age()<5000)line+=String(gps.satellites.value());
  if(sdReady)gpsLog.println(line);
  else Serial.println(String("GPS_CSV,")+line);
}
void readPackets(){
  if(!incoming)return;
  RXMessage r;
  int budget=24;
  while(budget-- && xQueueReceive(incoming,&r,0)==pdTRUE) {
    trailbridge::Motion m={};
    if(!trailbridge::decode(r.bytes,trailbridge::kPacketSize,m)){++invalidFrames;continue;}
    if(!peerTracker.accept(m))continue;
    received++;lastPacketMs=r.receivedMs;
    String line=String(r.receivedMs)+","+String(m.rideId)+","+String(m.sequence)+","+
      String(m.elapsedSeconds)+","+String(m.rmsMg)+","+String(m.peak10Mg)+","+
      String(m.impacts)+","+String(m.senderMs);
    if(sdReady)motionLog.println(line);
    else Serial.println(String("IMU_CSV,")+line);
  }
}
void drawUI(uint32_t now){
  auto& d=M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextSize(2);d.setTextColor(TFT_WHITE);
  d.setCursor(8,8);d.print("TRAIL / GPS");
  d.setTextSize(1);d.setTextColor(TFT_GREEN);
  d.setCursor(9,42);d.printf("GPS: %s",gps.location.isValid()&&gps.location.age()<2500?"FIX":"NO FIX");
  d.setCursor(9,58);d.printf("SD: %s",sdReady?gpsPath:"NO SD / SERIAL ONLY");
  d.setCursor(9,74);d.printf("ESPNOW: %s / CH 6",radioReady?"RX":"ERROR");
  d.setCursor(9,90);d.printf("RX: %lu   GAP: %lu",(unsigned long)received,(unsigned long)peerTracker.gaps());
  const String age=received?String((now-lastPacketMs)/1000)+"s":String("NONE");
  d.setCursor(9,106);d.printf("LAST: %s",age.c_str());
  d.setCursor(9,124);d.setTextColor(TFT_DARKGREY);d.print("GPS/IMU stored separately");
}
void setup(){
  Serial.begin(115200);
  auto cfg=M5.config(); M5Cardputer.begin(cfg,true);
  M5Cardputer.Display.setRotation(1);
  // Larger NMEA RX buffer before begin prevents 115200 baud bursts from overflowing.
  gpsUart.setRxBufferSize(4096);
  gpsUart.begin(115200,SERIAL_8N1,GPS_RX,GPS_TX);
  sdReady=openLogs();
  initEspNow();
  Serial.printf("GNSS RX=%d TX=%d @115200 | SD=%s | ESPNOW=%s\n",GPS_RX,GPS_TX,
     sdReady?"OK":"NO",radioReady?"RX":"FAIL");
  lastLog=millis();lastDraw=millis();lastFlushMs=millis();
}
void loop(){
  M5Cardputer.update();
  while(gpsUart.available())gps.encode((char)gpsUart.read());
  readPackets();
  uint32_t now=millis();
  if((uint32_t)(now-lastLog)>=1000){lastLog+=1000;logFix(now);}
  if(sdReady&&(uint32_t)(now-lastFlushMs)>=5000){lastFlushMs=now;gpsLog.flush();motionLog.flush();}
  if((uint32_t)(now-lastDraw)>=500){lastDraw=now;drawUI(now);}
  delay(1);
}
