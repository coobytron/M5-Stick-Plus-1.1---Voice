// Voice pen: a click-to-record dictation pen with an e-ink window.
//
// Click the top: the pen wakes and records to its microSD card. Click again:
// it stops, joins Wi-Fi, and hands every waiting recording to the dictation
// receiver on your Mac (mac/receiver.py), which transcribes it with Whisper
// and sends the text back. The start of the transcript stays on the e-ink
// window while the pen sleeps. With no Wi-Fi or no Mac in reach, recordings
// wait on the card and the pen retries every kRetryMinutes.
//
// Uses the same wire protocol as the M5StickC dictation firmware (docs/DICTATION.md),
// with two optional header fields:
//   ts=<unix seconds>  when the recording started (omitted if the clock isn't set)
//   reply=text         after "OK", the receiver sends "TEXT <transcript>\n"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <driver/i2s.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <freertos/stream_buffer.h>
#include <time.h>

#include <vector>

#include "config.h"
#include "screen.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy src/secrets.example.h to src/secrets.h and fill in your Wi-Fi details."
#endif

namespace {

constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr size_t kWavHeaderBytes = 44;
constexpr const char* kRecDir = "/rec";
constexpr const char* kLastTextPath = "/last.txt";

struct WifiNetwork {
  const char* ssid;
  const char* pass;
};
const WifiNetwork kNetworks[] = WIFI_NETWORKS;

StreamBufferHandle_t audioStream;
volatile bool capturing = false;
volatile int16_t lastPeak = 0;
volatile uint32_t droppedBytes = 0;

// Clicks are latched by an interrupt so none are missed while the e-ink
// refresh blocks the main loop.
volatile uint32_t clicks = 0;
volatile uint32_t lastEdgeMs = 0;

String lastTranscript;

// ---------------------------------------------------------------------------
// Button and battery

void IRAM_ATTR onButton() {
  uint32_t now = millis();
  if (now - lastEdgeMs > kDebounceMs) clicks++;
  lastEdgeMs = now;
}

bool buttonDown() { return digitalRead(kButtonPin) == LOW; }

void waitForRelease() {
  while (buttonDown()) delay(10);
  delay(kDebounceMs);
}

int batteryMv() { return analogReadMilliVolts(kBatteryPin) * 2; }

int batteryPercent() {
  return map(constrain(batteryMv(), kBatteryEmptyMv, kBatteryFullMv), kBatteryEmptyMv, kBatteryFullMv, 0, 100);
}

// ---------------------------------------------------------------------------
// Microphone (same clean-up as the M5StickC dictation firmware)

void micBegin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  cfg.sample_rate = kSampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
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
}

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
// Recordings on the card: /rec/<seq>_<unix time or 0>.wav

void writeWavHeader(File& f, uint32_t dataBytes) {
  uint32_t byteRate = kSampleRate * 2;
  uint8_t h[kWavHeaderBytes];
  auto put32 = [&](int at, uint32_t v) { memcpy(h + at, &v, 4); };
  auto put16 = [&](int at, uint16_t v) { memcpy(h + at, &v, 2); };
  memcpy(h, "RIFF", 4);
  put32(4, 36 + dataBytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(16, 16);
  put16(20, 1);  // PCM
  put16(22, 1);  // mono
  put32(24, kSampleRate);
  put32(28, byteRate);
  put16(32, 2);
  put16(34, 16);
  memcpy(h + 36, "data", 4);
  put32(40, dataBytes);
  f.seek(0);
  f.write(h, sizeof(h));
}

bool clockIsSet() { return time(nullptr) > 1700000000; }

// Oldest first, which is also upload order.
std::vector<String> pendingRecordings() {
  std::vector<String> names;
  File dir = SD.open(kRecDir);
  if (!dir) return names;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String name = f.name();
    if (!f.isDirectory() && name.endsWith(".wav")) names.push_back(name);
  }
  std::sort(names.begin(), names.end(), [](const String& a, const String& b) { return a.toInt() < b.toInt(); });
  return names;
}

uint32_t nextSequence() {
  auto names = pendingRecordings();
  return names.empty() ? 1 : names.back().toInt() + 1;
}

void loadLastTranscript() {
  File f = SD.open(kLastTextPath);
  if (f) lastTranscript = f.readString();
}

void saveLastTranscript(const String& text) {
  lastTranscript = text;
  File f = SD.open(kLastTextPath, FILE_WRITE);
  if (f) f.print(text);
}

// Records until the button is clicked again. The mic is already capturing
// (setup starts it at wake), so words spoken during boot are kept.
// Returns false if nothing usable was saved.
bool record() {
  SD.mkdir(kRecDir);
  char path[48];
  snprintf(path, sizeof(path), "%s/%05u_%lu.wav", kRecDir, (unsigned)nextSequence(),
           clockIsSet() ? (unsigned long)time(nullptr) : 0UL);
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    capturing = false;
    screen::message("Card error", "Couldn't create a file on the microSD card.");
    return false;
  }
  writeWavHeader(f, 0);

  waitForRelease();  // the press that woke us is not the stop click
  clicks = 0;
  attachInterrupt(kButtonPin, onButton, FALLING);

  static uint8_t chunk[4096];
  uint32_t dataBytes = 0;
  uint32_t lastDrawnSecond = UINT32_MAX;
  const char* stopReason = nullptr;
  while (!stopReason) {
    size_t n = xStreamBufferReceive(audioStream, chunk, sizeof(chunk), pdMS_TO_TICKS(50));
    if (n > 0) {
      if (f.write(chunk, n) != n) stopReason = "Card full";
      dataBytes += n;
    }
    uint32_t seconds = millis() / 1000;  // millis() counts from wake, which was the click
    if (seconds != lastDrawnSecond) {
      lastDrawnSecond = seconds;
      screen::recording(seconds, min<int>(lastPeak, 20000) / 200);
    }
    if (clicks > 0) stopReason = "";
    else if (seconds >= kMaxRecordSeconds) stopReason = "Time limit";
    else if (seconds % 10 == 0 && batteryMv() < kBatteryEmptyMv) stopReason = "Battery low";
  }

  detachInterrupt(kButtonPin);
  capturing = false;
  // Keep what the buffer still holds.
  while (size_t n = xStreamBufferReceive(audioStream, chunk, sizeof(chunk), 0)) {
    if (f.write(chunk, n) != n) break;
    dataBytes += n;
  }
  writeWavHeader(f, dataBytes);
  f.close();
  if (droppedBytes > 0) Serial.printf("dropped %u bytes: card too slow\n", (unsigned)droppedBytes);

  // Under half a second is a mis-click, not a thought.
  if (dataBytes < kSampleRate) {
    SD.remove(path);
    return false;
  }
  if (*stopReason) screen::message(stopReason, "Recording saved.");
  return true;
}

// ---------------------------------------------------------------------------
// Sync with the Mac

WiFiClient client;
IPAddress receiverIp;
uint16_t receiverPort = DICTATION_PORT;

bool joinWifi() {
  WiFiMulti wifiMulti;
  for (const auto& net : kNetworks) wifiMulti.addAP(net.ssid, net.pass);
  WiFi.mode(WIFI_STA);
  return wifiMulti.run(kWifiTimeoutMs) == WL_CONNECTED;
}

bool findReceiver() {
  if (strlen(DICTATION_HOST) > 0) {
    return receiverIp.fromString(DICTATION_HOST) || WiFi.hostByName(DICTATION_HOST, receiverIp);
  }
  MDNS.begin("voicepen");
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

// Sends one recording. On success the receiver has it, and if wantText,
// transcript holds what came back (empty if it took too long).
bool upload(const String& name, bool wantText, String& transcript) {
  String path = String(kRecDir) + "/" + name;
  File f = SD.open(path);
  if (!f || f.size() <= kWavHeaderBytes) return false;
  unsigned long ts = strtoul(name.substring(name.indexOf('_') + 1).c_str(), nullptr, 10);

  if (!client.connect(receiverIp, receiverPort, 3000)) return false;
  client.printf("M5DICT/1 rate=%u bits=16 ch=1", (unsigned)kSampleRate);
  if (ts > 0) client.printf(" ts=%lu", ts);
  client.print(wantText ? " reply=text\n" : "\n");

  static uint8_t chunk[2048];
  f.seek(kWavHeaderBytes);
  bool ok = true;
  while (ok) {
    int n = f.read(chunk, sizeof(chunk));
    if (n <= 0) break;
    ok = sendFrame(chunk, n);
  }
  f.close();
  ok = ok && sendFrame(nullptr, 0);

  client.setTimeout(10);
  ok = ok && client.readStringUntil('\n').startsWith("OK");
  if (ok) {
    SD.remove(path);  // the Mac has it now
    if (wantText) {
      client.setTimeout(kTranscriptWaitMs / 1000);
      String line = client.readStringUntil('\n');
      if (line.startsWith("TEXT")) transcript = line.substring(4);
      transcript.trim();
    }
  }
  client.stop();
  return ok;
}

// Uploads everything waiting. Returns a short reason when something is left.
const char* syncAll() {
  auto names = pendingRecordings();
  if (names.empty()) return nullptr;

  screen::message("Sending", String(names.size()) + (names.size() == 1 ? " recording" : " recordings"));
  if (!joinWifi()) return "No Wi-Fi";
  configTime(0, 0, "pool.ntp.org");  // timestamps for the next recordings
  if (!findReceiver()) return "Mac not found";

  for (size_t i = 0; i < names.size(); i++) {
    bool newest = i == names.size() - 1;
    if (newest) screen::message("Transcribing", "on your Mac...");
    String transcript;
    if (!upload(names[i], newest, transcript)) return "Upload failed";
    if (transcript.length() > 0) saveLastTranscript(transcript);
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Sleep

[[noreturn]] void sleepNow(int waiting) {
  screen::home(lastTranscript, waiting, batteryPercent());
  screen::sleep();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  SD.end();

  waitForRelease();  // a held button would wake us straight away
  rtc_gpio_pullup_en((gpio_num_t)kButtonPin);
  rtc_gpio_pulldown_dis((gpio_num_t)kButtonPin);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)kButtonPin, 0);
  if (waiting > 0) esp_sleep_enable_timer_wakeup((uint64_t)kRetryMinutes * 60 * 1000000ULL);
  esp_deep_sleep_start();
}

}  // namespace

// ---------------------------------------------------------------------------

void setup() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Serial.begin(115200);
  rtc_gpio_deinit((gpio_num_t)kButtonPin);  // back from wake-up duty to a normal input
  pinMode(kButtonPin, INPUT_PULLUP);
  analogSetPinAttenuation(kBatteryPin, ADC_11db);

  // Start listening first: on a button wake, speech may already be coming.
  audioStream = xStreamBufferCreate(kAudioBufferBytes, 1);
  micBegin();
  xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, 5, nullptr, 0);
  bool clickedAwake = cause == ESP_SLEEP_WAKEUP_EXT0;
  capturing = clickedAwake;

  SPI.begin(kSpiSckPin, kSpiMisoPin, kSpiMosiPin);
  screen::begin(cause != ESP_SLEEP_WAKEUP_UNDEFINED);

  if (!SD.begin(kSdCsPin, SPI)) {
    screen::message("No card", "Insert a microSD card, then click.");
    capturing = false;
    waitForRelease();
    rtc_gpio_pullup_en((gpio_num_t)kButtonPin);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)kButtonPin, 0);
    esp_deep_sleep_start();
  }
  loadLastTranscript();

  if (clickedAwake) record();
  capturing = false;

  const char* problem = syncAll();
  int waiting = pendingRecordings().size();
  if (problem) {
    screen::message(problem, "Saved on the pen. It will try again later.");
    delay(1500);  // long enough to read before the home screen replaces it
  }
  sleepNow(waiting);
}

void loop() {}
