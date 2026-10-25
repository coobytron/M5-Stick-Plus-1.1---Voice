// Pocket voice recorder: one button, an e-ink window, transcripts from your Mac.
//
// Press the button: the recorder wakes and records to its microSD card. Press
// again: it stops, shows "Saved", joins Wi-Fi and hands every waiting
// recording to the dictation receiver on your Mac (mac/receiver.py), which
// transcribes it with Whisper and sends the text back. The first words become
// the note's title in Files. Hold the button instead of pressing it to open
// Files. With no Wi-Fi or no Mac in reach, recordings wait on the card and the
// recorder retries every kRetryMinutes.
//
// Wire protocol: see docs/RECEIVER.md. This firmware sends ts= (when the clock
// is set) and reply=text on every upload.

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

#include <algorithm>
#include <vector>

#include "config.h"
#include "notes.h"
#include "screen.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy src/secrets.example.h to src/secrets.h and fill in your Wi-Fi details."
#endif

#ifndef TIMEZONE
#define TIMEZONE "UTC0"  // secrets.h from before the clock existed
#endif

namespace {

constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr size_t kWavHeaderBytes = 44;
constexpr const char* kRecDir = "/rec";
constexpr uint32_t kLevelStepMs = 250;  // one waveform bar per quarter second
constexpr size_t kLevelBars = 28;

struct WifiNetwork {
  const char* ssid;
  const char* pass;
};
const WifiNetwork kNetworks[] = WIFI_NETWORKS;

StreamBufferHandle_t audioStream;
volatile bool capturing = false;
volatile int16_t lastPeak = 0;
volatile uint32_t droppedBytes = 0;

// Presses while recording are latched by an interrupt so none are missed
// while the e-ink refresh blocks the main loop.
volatile uint32_t clicks = 0;
volatile uint32_t lastEdgeMs = 0;

// ---------------------------------------------------------------------------
// Button, LED and battery

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

enum class Press { None, Short, Long };

// Waits up to timeoutMs for a press and tells a short press from a hold.
Press waitPress(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (!buttonDown()) {
    if (millis() - start > timeoutMs) return Press::None;
    delay(10);
  }
  uint32_t down = millis();
  delay(kDebounceMs);
  while (buttonDown()) {
    if (millis() - down >= kLongPressMs) {
      waitForRelease();
      return Press::Long;
    }
    delay(10);
  }
  return Press::Short;
}

void led(bool on) { digitalWrite(kLedPin, on ? HIGH : LOW); }

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
  uint32_t seq = 0;
  for (const auto& n : notes::load()) seq = max(seq, n.seq);
  auto names = pendingRecordings();
  if (!names.empty()) seq = max<uint32_t>(seq, names.back().toInt());
  return seq + 1;
}

enum class Recorded { Saved, Nothing, OpenFiles };

// Records until the button is pressed again. The mic is already capturing
// (setup starts it at wake), so words spoken during boot are kept. Holding
// the wake press instead of letting go means "open Files".
Recorded record(Note& note) {
  while (buttonDown()) {
    if (millis() >= kLongPressMs) {  // millis() counts from wake, which was the press
      capturing = false;
      return Recorded::OpenFiles;
    }
    delay(5);
  }
  led(true);

  SD.mkdir(kRecDir);
  note.seq = nextSequence();
  note.ts = clockIsSet() ? time(nullptr) - millis() / 1000 : 0;
  char path[48];
  snprintf(path, sizeof(path), "%s/%05u_%lu.wav", kRecDir, (unsigned)note.seq, (unsigned long)note.ts);
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    capturing = false;
    led(false);
    screen::message("Card error", "Check the microSD card.");
    return Recorded::Nothing;
  }
  writeWavHeader(f, 0);

  delay(kDebounceMs);
  clicks = 0;
  attachInterrupt(kButtonPin, onButton, FALLING);

  static uint8_t chunk[4096];
  std::vector<uint8_t> levels(kLevelBars, 0);
  int16_t stepPeak = 0;
  uint32_t nextStepMs = millis() + kLevelStepMs;
  uint32_t dataBytes = 0;
  uint32_t lastDrawnSecond = UINT32_MAX;
  const char* stopReason = nullptr;
  while (!stopReason) {
    size_t n = xStreamBufferReceive(audioStream, chunk, sizeof(chunk), pdMS_TO_TICKS(50));
    if (n > 0) {
      if (f.write(chunk, n) != n) stopReason = "Card full";
      dataBytes += n;
    }
    stepPeak = max<int16_t>(stepPeak, (int16_t)lastPeak);
    if (millis() >= nextStepMs) {
      levels.erase(levels.begin());
      levels.push_back(min<int>(stepPeak, 20000) / 200);
      stepPeak = 0;
      nextStepMs += kLevelStepMs;
    }
    uint32_t seconds = millis() / 1000;
    if (seconds != lastDrawnSecond) {
      lastDrawnSecond = seconds;
      screen::recording(seconds, levels, batteryPercent());
    }
    if (clicks > 0) stopReason = "";
    else if (seconds >= kMaxRecordSeconds) stopReason = "Time limit";
    else if (seconds % 10 == 0 && batteryMv() < kBatteryEmptyMv) stopReason = "Battery low";
  }

  detachInterrupt(kButtonPin);
  capturing = false;
  led(false);
  // Keep what the buffer still holds.
  while (size_t n = xStreamBufferReceive(audioStream, chunk, sizeof(chunk), 0)) {
    if (f.write(chunk, n) != n) break;
    dataBytes += n;
  }
  writeWavHeader(f, dataBytes);
  f.close();
  if (droppedBytes > 0) Serial.printf("dropped %u bytes: card too slow\n", (unsigned)droppedBytes);

  // Under half a second is a mis-press, not a thought.
  if (dataBytes < kSampleRate) {
    SD.remove(path);
    return Recorded::Nothing;
  }
  note.seconds = dataBytes / (kSampleRate * 2);
  notes::add(note);
  if (*stopReason) screen::message(stopReason, "Recording saved.");
  return Recorded::Saved;
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
  MDNS.begin("voicerecorder");
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

// Sends one recording and waits for its transcript. On success the Mac has
// it; transcript is empty if transcription took too long or found no speech.
bool upload(const String& name, String& transcript) {
  String path = String(kRecDir) + "/" + name;
  File f = SD.open(path);
  if (!f || f.size() <= kWavHeaderBytes) return false;
  unsigned long ts = strtoul(name.substring(name.indexOf('_') + 1).c_str(), nullptr, 10);

  if (!client.connect(receiverIp, receiverPort, 3000)) return false;
  client.printf("M5DICT/1 rate=%u bits=16 ch=1", (unsigned)kSampleRate);
  if (ts > 0) client.printf(" ts=%lu", ts);
  client.print(" reply=text\n");

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
    client.setTimeout(kTranscriptWaitMs / 1000);
    String line = client.readStringUntil('\n');
    if (line.startsWith("TEXT")) transcript = line.substring(4);
    transcript.trim();
  }
  client.stop();
  return ok;
}

// Uploads everything waiting. If shown is set, its "Saved" screen carries the
// progress and ends with its transcript title. Returns a short reason when
// something is left on the card.
const char* syncAll(Note* shown) {
  auto names = pendingRecordings();
  if (names.empty()) return nullptr;
  auto status = [&](const String& s) {
    if (shown) screen::saved(*shown, s, batteryPercent());
  };

  status("Joining Wi-Fi...");
  if (!joinWifi()) return "No Wi-Fi";
  configTime(0, 0, "pool.ntp.org");  // dates for the next recordings
  if (!findReceiver()) return "Mac not found";

  for (size_t i = 0; i < names.size(); i++) {
    uint32_t seq = names[i].toInt();
    bool isShown = shown && seq == shown->seq;
    status(isShown ? String("Transcribing...") : String("Sending ") + (i + 1) + " of " + names.size());
    String transcript;
    if (!upload(names[i], transcript)) return "Upload failed";
    notes::markSent(seq, transcript);
    if (isShown) {
      shown->sent = true;
      shown->text = transcript;
    }
  }
  status("Synced");
  return nullptr;
}

// ---------------------------------------------------------------------------
// Files: press to move down, hold to open a note (or leave on "Back").

void browseFiles() {
  auto list = notes::load();
  int selected = 0;
  bool inDetail = false;
  screen::files(list, selected, batteryPercent());
  for (;;) {
    Press p = waitPress(kFilesIdleMs);
    if (p == Press::None) return;
    if (inDetail) {
      inDetail = false;
    } else if (p == Press::Short) {
      selected = (selected + 1) % (list.size() + 1);
    } else if (selected == (int)list.size()) {
      return;  // "Back"
    } else {
      inDetail = true;
      screen::detail(list[selected], batteryPercent());
      continue;
    }
    screen::files(list, selected, batteryPercent());
  }
}

// ---------------------------------------------------------------------------
// Sleep

[[noreturn]] void sleepNow(int waiting) {
  screen::idle(batteryPercent(), waiting);
  screen::sleep();
  led(false);
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
  pinMode(kLedPin, OUTPUT);
  led(false);
  analogSetPinAttenuation(kBatteryPin, ADC_11db);
  setenv("TZ", TIMEZONE, 1);  // the clock itself survives deep sleep; the zone doesn't
  tzset();

  // Start listening first: on a button wake, speech may already be coming.
  audioStream = xStreamBufferCreate(kAudioBufferBytes, 1);
  micBegin();
  xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, 5, nullptr, 0);
  bool pressedAwake = cause == ESP_SLEEP_WAKEUP_EXT0;
  capturing = pressedAwake;

  SPI.begin(kSpiSckPin, kSpiMisoPin, kSpiMosiPin);
  screen::begin(cause != ESP_SLEEP_WAKEUP_UNDEFINED);

  if (!SD.begin(kSdCsPin, SPI)) {
    capturing = false;
    screen::message("No card", "Insert a microSD card.");
    waitForRelease();
    rtc_gpio_pullup_en((gpio_num_t)kButtonPin);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)kButtonPin, 0);
    esp_deep_sleep_start();
  }

  Note note;
  Recorded result = pressedAwake ? record(note) : Recorded::Nothing;
  capturing = false;

  if (result == Recorded::OpenFiles) {
    browseFiles();
    sleepNow(pendingRecordings().size());
  }

  bool fresh = result == Recorded::Saved;
  if (fresh) {
    screen::saved(note, "", batteryPercent());
    delay(800);  // let "Saved" land before Wi-Fi status replaces the footer
  }
  const char* problem = syncAll(fresh ? &note : nullptr);
  if (fresh) {
    // The idle screen's "N to sync" says it's kept; the footer only has room for the reason.
    screen::saved(note, problem ? String(problem) : String("Synced"), batteryPercent());
    delay(kSavedShowMs);
  }
  sleepNow(pendingRecordings().size());
}

void loop() {}
