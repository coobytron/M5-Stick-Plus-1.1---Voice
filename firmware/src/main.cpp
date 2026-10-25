// M5StickC Plus 1.1 dictation recorder.
//
// Press button A to start recording, press it again to stop. While recording,
// audio from the built-in PDM mic is streamed over Wi-Fi to the dictation
// receiver on your Mac, which turns it into a transcript.
//
// Wire protocol (TCP, port DICTATION_PORT):
//   1. Stick sends one header line:  "M5DICT/1 rate=16000 bits=16 ch=1\n"
//   2. Then frames of [uint16 little-endian length][length bytes of PCM]
//      (signed 16-bit little-endian mono samples).
//   3. A frame with length 0 ends the recording.
//   4. Receiver answers with a line starting "OK" once it has the audio.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <driver/i2s.h>
#include <freertos/stream_buffer.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy src/secrets.example.h to src/secrets.h and fill in your Wi-Fi details."
#endif

namespace {

constexpr uint32_t kSampleRate = 16000;
constexpr int kMicClkPin = 0;    // SPM1423 PDM clock
constexpr int kMicDataPin = 34;  // SPM1423 PDM data
constexpr int kLedPin = 10;      // red LED, active low
constexpr i2s_port_t kI2sPort = I2S_NUM_0;

// The PDM mic is quiet; boost it before sending. Raise if transcripts miss
// soft speech, lower if loud speech sounds distorted.
constexpr int kMicGain = 8;

// ~1.5 s of audio, so short Wi-Fi hiccups don't drop speech.
constexpr size_t kStreamBufferBytes = 48 * 1024;
constexpr size_t kSendChunkBytes = 2048;

struct WifiNetwork {
  const char* ssid;
  const char* pass;
};
const WifiNetwork kNetworks[] = WIFI_NETWORKS;

enum class State { ConnectingWifi, FindingMac, Ready, Recording, Finishing, Sent, Error };

WiFiMulti wifiMulti;
WiFiClient client;
StreamBufferHandle_t audioStream;
M5Canvas canvas(&M5.Display);  // draw off-screen, then push, to avoid flicker

State state = State::ConnectingWifi;
String errorText;
IPAddress receiverIp;
uint16_t receiverPort = DICTATION_PORT;

volatile bool capturing = false;
volatile int16_t lastPeak = 0;
volatile uint32_t droppedBytes = 0;
uint32_t recordStartMs = 0;
uint32_t sentBytes = 0;
uint32_t stateChangedMs = 0;

// ---------------------------------------------------------------------------
// Microphone

void micBegin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  cfg.sample_rate = kSampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ALL_RIGHT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = I2S_PIN_NO_CHANGE;
  pins.ws_io_num = kMicClkPin;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = kMicDataPin;

  i2s_driver_install(kI2sPort, &cfg, 0, nullptr);
  i2s_set_pin(kI2sPort, &pins);
  i2s_set_clk(kI2sPort, kSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
}

// Reads the mic continuously. Samples are cleaned up (DC offset removed, gain
// applied) and pushed into audioStream only while capturing is true.
void micTask(void*) {
  static int16_t samples[256];
  int32_t dcEstimate = 0;  // running mean, Q8 fixed point

  for (;;) {
    size_t bytesRead = 0;
    i2s_read(kI2sPort, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
    size_t count = bytesRead / sizeof(int16_t);

    int16_t peak = 0;
    for (size_t i = 0; i < count; i++) {
      int32_t s = (int32_t)samples[i] << 8;
      dcEstimate += (s - dcEstimate) >> 10;
      int32_t v = ((s - dcEstimate) >> 8) * kMicGain;
      if (v > INT16_MAX) v = INT16_MAX;
      if (v < INT16_MIN) v = INT16_MIN;
      samples[i] = (int16_t)v;
      int16_t mag = v < 0 ? -v : v;
      if (mag > peak) peak = mag;
    }
    lastPeak = peak;

    if (capturing) {
      size_t want = count * sizeof(int16_t);
      size_t put = xStreamBufferSend(audioStream, samples, want, 0);
      if (put < want) droppedBytes += want - put;
    }
  }
}

// ---------------------------------------------------------------------------
// Network

bool findReceiver() {
  if (strlen(DICTATION_HOST) > 0) {
    return receiverIp.fromString(DICTATION_HOST) || WiFi.hostByName(DICTATION_HOST, receiverIp);
  }
  int found = MDNS.queryService("m5dict", "tcp");
  if (found <= 0) return false;
  receiverIp = MDNS.IP(0);
  receiverPort = MDNS.port(0);
  return true;
}

bool sendFrame(const uint8_t* data, uint16_t len) {
  uint8_t header[2] = {(uint8_t)(len & 0xff), (uint8_t)(len >> 8)};
  if (client.write(header, 2) != 2) return false;
  return len == 0 || client.write(data, len) == len;
}

void setState(State s) {
  state = s;
  stateChangedMs = millis();
}

void fail(const String& why) {
  capturing = false;
  digitalWrite(kLedPin, HIGH);
  client.stop();
  errorText = why;
  setState(State::Error);
}

void startRecording() {
  if (!client.connect(receiverIp, receiverPort, 3000)) {
    fail("Can't reach Mac");
    return;
  }
  client.setNoDelay(true);
  client.printf("M5DICT/1 rate=%u bits=16 ch=1\n", (unsigned)kSampleRate);

  xStreamBufferReset(audioStream);
  droppedBytes = 0;
  sentBytes = 0;
  recordStartMs = millis();
  capturing = true;
  digitalWrite(kLedPin, LOW);
  setState(State::Recording);
}

// Moves buffered audio to the network. Returns false if the connection broke.
bool pumpAudio(TickType_t wait) {
  static uint8_t chunk[kSendChunkBytes];
  size_t n = xStreamBufferReceive(audioStream, chunk, sizeof(chunk), wait);
  if (n == 0) return client.connected();
  if (!sendFrame(chunk, n)) return false;
  sentBytes += n;
  return true;
}

void stopRecording() {
  capturing = false;
  digitalWrite(kLedPin, HIGH);
  setState(State::Finishing);

  while (xStreamBufferBytesAvailable(audioStream) > 0) {
    if (!pumpAudio(0)) {
      fail("Lost connection");
      return;
    }
  }
  if (!sendFrame(nullptr, 0)) {
    fail("Lost connection");
    return;
  }

  // Wait for the receiver to confirm it has the audio.
  client.setTimeout(10);
  String reply = client.readStringUntil('\n');
  client.stop();
  if (!reply.startsWith("OK")) {
    fail("Mac didn't confirm");
    return;
  }
  setState(State::Sent);
}

// ---------------------------------------------------------------------------
// Screen

void drawScreen() {
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  auto& d = canvas;
  uint16_t bg = TFT_BLACK;
  if (state == State::Recording) bg = TFT_RED;
  if (state == State::Error) bg = TFT_MAROON;

  d.fillScreen(bg);
  d.setTextColor(TFT_WHITE, bg);

  // Status bar: Wi-Fi and battery.
  d.setTextSize(1);
  d.setCursor(4, 4);
  d.print(WiFi.isConnected() ? WiFi.SSID() : String("no Wi-Fi"));
  int battery = M5.Power.getBatteryLevel();
  d.setCursor(d.width() - 40, 4);
  d.printf("%3d%%", battery < 0 ? 0 : battery);

  d.setTextSize(3);
  d.setCursor(8, 30);
  switch (state) {
    case State::ConnectingWifi:
      d.print("Wi-Fi...");
      break;
    case State::FindingMac:
      d.print("Find Mac");
      d.setTextSize(1);
      d.setCursor(8, 70);
      d.print("Start the receiver on your Mac");
      break;
    case State::Ready:
      d.print("Ready");
      d.setTextSize(2);
      d.setCursor(8, 80);
      d.print("A = record");
      break;
    case State::Recording: {
      uint32_t secs = (millis() - recordStartMs) / 1000;
      d.printf("REC %02u:%02u", (unsigned)(secs / 60), (unsigned)(secs % 60));
      // Level meter so you can see the mic is hearing you.
      int w = map(min<int>(lastPeak, 20000), 0, 20000, 0, d.width() - 16);
      d.fillRect(8, 75, w, 12, TFT_WHITE);
      d.drawRect(8, 75, d.width() - 16, 12, TFT_WHITE);
      d.setTextSize(2);
      d.setCursor(8, 100);
      d.print("A = stop");
      if (droppedBytes > 0) {
        d.setTextSize(1);
        d.setCursor(150, 108);
        d.print("weak Wi-Fi");
      }
      break;
    }
    case State::Finishing:
      d.print("Sending");
      break;
    case State::Sent:
      d.print("Sent!");
      d.setTextSize(2);
      d.setCursor(8, 80);
      d.printf("%u sec", (unsigned)(sentBytes / (kSampleRate * 2)));
      break;
    case State::Error:
      d.setTextSize(2);
      d.print(errorText);
      d.setCursor(8, 80);
      d.print("A = retry");
      break;
  }
  d.pushSprite(0, 0);
}

}  // namespace

// ---------------------------------------------------------------------------

void setup() {
  auto cfg = M5.config();
  cfg.internal_mic = false;  // we drive the mic ourselves over I2S
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(80);
  canvas.setColorDepth(8);
  canvas.createSprite(M5.Display.width(), M5.Display.height());

  pinMode(kLedPin, OUTPUT);
  digitalWrite(kLedPin, HIGH);

  audioStream = xStreamBufferCreate(kStreamBufferBytes, 1);
  micBegin();
  xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, 5, nullptr, 0);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // power save adds latency spikes that overflow the buffer
  for (const auto& net : kNetworks) wifiMulti.addAP(net.ssid, net.pass);
  MDNS.begin("m5dictation");
  setState(State::ConnectingWifi);
}

void loop() {
  M5.update();
  bool pressA = M5.BtnA.wasPressed();

  switch (state) {
    case State::ConnectingWifi:
      if (wifiMulti.run(5000) == WL_CONNECTED) setState(State::FindingMac);
      break;

    case State::FindingMac:
      if (findReceiver()) setState(State::Ready);
      else delay(1000);
      break;

    case State::Ready:
    case State::Sent:
      if (!WiFi.isConnected()) setState(State::ConnectingWifi);
      else if (pressA) startRecording();
      break;

    case State::Recording:
      if (pressA) stopRecording();
      else if (!pumpAudio(pdMS_TO_TICKS(20))) fail("Lost connection");
      break;

    case State::Finishing:
      break;

    case State::Error:
      if (pressA) setState(WiFi.isConnected() ? State::FindingMac : State::ConnectingWifi);
      break;
  }

  drawScreen();
  if (state != State::Recording) delay(10);
}
