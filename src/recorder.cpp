#include "recorder.h"

#include <LittleFS.h>
#include <M5Unified.h>

#include <algorithm>

#include "config.h"
#include "wav.h"

namespace recorder {
namespace {

// Three rotating 100 ms buffers. M5.Mic holds at most two at a time (one
// filling, one queued), so after queuing buffer n, buffer n-2 is complete
// and safe to write out.
constexpr size_t BUFFER_SAMPLES = SAMPLE_RATE / 10;
constexpr int BUFFER_COUNT = 3;
int16_t buffers[BUFFER_COUNT][BUFFER_SAMPLES];

File file;
bool recording = false;
uint32_t queued = 0;  // buffers handed to the mic
uint32_t written = 0;  // buffers written to the file
uint32_t max_buffers = 0;
float last_level = 0;

void writeBuffer(uint32_t index) {
  const int16_t* buf = buffers[index % BUFFER_COUNT];
  int16_t peak = 0;
  for (size_t i = 0; i < BUFFER_SAMPLES; ++i) {
    int16_t v = buf[i] < 0 ? -buf[i] : buf[i];
    if (v > peak) peak = v;
  }
  last_level = peak / 32768.0f;
  file.write(reinterpret_cast<const uint8_t*>(buf), sizeof(buffers[0]));
  ++written;
}

}  // namespace

bool begin() {
  if (!LittleFS.begin(true)) return false;
  // The Plus's buzzer and mic share the I2S peripheral.
  M5.Speaker.end();
  auto cfg = M5.Mic.config();
  cfg.sample_rate = SAMPLE_RATE;
  M5.Mic.config(cfg);
  return M5.Mic.begin();
}

bool start() {
  LittleFS.remove(RECORDING_PATH);
  size_t free_bytes = LittleFS.totalBytes() - LittleFS.usedBytes();
  // Leave a margin for LittleFS metadata.
  size_t room = free_bytes > 32768 ? free_bytes - 32768 : 0;
  max_buffers = std::min<uint32_t>(MAX_RECORD_SECONDS * 10, room / sizeof(buffers[0]));
  if (max_buffers < MIN_RECORD_MS / 100) return false;

  file = LittleFS.open(RECORDING_PATH, "w");
  if (!file) return false;
  WavHeader header(SAMPLE_RATE, 0);
  file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));

  queued = written = 0;
  last_level = 0;
  recording = true;
  return true;
}

bool poll() {
  if (!recording) return false;
  if (queued >= max_buffers) return false;
  // Blocks until the mic has room in its queue (at most ~100 ms).
  M5.Mic.record(buffers[queued % BUFFER_COUNT], BUFFER_SAMPLES, SAMPLE_RATE);
  ++queued;
  if (queued >= 3) writeBuffer(queued - 3);
  return queued < max_buffers;
}

uint32_t stop() {
  if (!recording) return 0;
  recording = false;
  while (M5.Mic.isRecording()) delay(1);
  while (written < queued) writeBuffer(written);
  file.close();

  uint32_t pcm_bytes = written * sizeof(buffers[0]);
  File f = LittleFS.open(RECORDING_PATH, "r+");
  if (f) {
    WavHeader header(SAMPLE_RATE, pcm_bytes);
    f.seek(0);
    f.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
    f.close();
  }
  return written * 100;
}

bool active() { return recording; }
uint32_t elapsedMs() { return queued * 100; }
uint32_t maxMs() { return max_buffers * 100; }
float level() { return last_level; }

}  // namespace recorder
