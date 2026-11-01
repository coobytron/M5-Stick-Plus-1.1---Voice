#pragma once
// Independent Signal OS Trail Echo capture/storage mode. Requires ESP32 Arduino + SPIFFS.
// Does not include M5Unified or own the device's buttons/menu.
#include <Arduino.h>
#include <SPIFFS.h>
#include "TrailEchoCore.h"

namespace trailecho {
class Mode {
 public:
  typedef bool (*AccelReader)(float& ax, float& ay, float& az);

  bool begin() {
    storageReady_ = SPIFFS.begin(false); // NEVER format a user's filesystem.
    if (!storageReady_) return false;
    // A prior brownout/aborted session is retained rather than silently erased.
    if (SPIFFS.exists(activePath())) {
      if (SPIFFS.exists(recoveredPath())) SPIFFS.remove(recoveredPath());
      if (!SPIFFS.rename(activePath(), recoveredPath())) writeError_ = true;
    }
    if (!load(lastPath())) load(recoveredPath());
    return true;
  }

  // Call when Signal OS enters the Trail Echo mode. Does not start recording.
  void enter() { screen_ = 0; }

  // Call continuously while Trail Echo is active. No Wi-Fi scanning in this mode.
  void update(uint32_t nowMs, AccelReader readAccel) {
    if (!core_.recording()) return;
    if (int32_t(nowMs - nextSample_) >= 0) {
      nextSample_ = nowMs + 20; // 50 Hz, no catch-up sampling with fake data
      float x=0, y=0, z=0;
      if (readAccel && readAccel(x, y, z)) {
        core_.sample(x, y, z, nowMs);
      } else {
        ++readErrors_;
      }
    }
    core_.advance(nowMs);
    flushBins();
    if (!core_.recording()) finalizeFile(); // capacity automatically reached
  }

  // Short A: start/stop. Long A stays owned by Signal OS and exits this mode.
  void shortA(uint32_t nowMs) {
    if (core_.recording()) {
      core_.stop(nowMs);
      flushBins();
      finalizeFile();
    } else {
      start(nowMs);
    }
  }

  // B toggles the score and stats screens after a ride.
  void shortB() { if (!core_.recording()) screen_ = uint8_t((screen_ + 1) % 2); }

  // Always call before leaving the mode so a ride cannot be silently discarded.
  void exit(uint32_t nowMs) {
    if (core_.recording()) { core_.stop(nowMs); flushBins(); finalizeFile(); }
  }

  bool recording() const { return core_.recording(); }
  uint16_t seconds() const { return core_.count(); }
  uint32_t sensorErrors() const { return readErrors_; }
  bool stored() const { return storageReady_ && !writeError_; }

  const Core& data() const { return core_; }
  uint8_t page() const { return screen_; }


 private:
  static const char* activePath() { return "/te-active.bin"; }
  static const char* lastPath() { return "/te-last.bin"; }
  static const char* recoveredPath() { return "/te-recovered.bin"; }
  static const uint8_t* magic() {
    static const uint8_t bytes[8] = {'T','R','A','I','L','0','1',0};
    return bytes;
  }

  void start(uint32_t nowMs) {
    if (file_) file_.close();
    core_.start(nowMs);
    nextSample_ = nowMs;
    writtenBins_ = 0;
    readErrors_ = 0;
    writeError_ = false;
    screen_ = 0;
    if (!storageReady_) return;
    SPIFFS.remove(activePath());
    file_ = SPIFFS.open(activePath(), FILE_WRITE);
    if (!file_ || file_.write(magic(), 8) != 8) {
      writeError_ = true;
      if (file_) file_.close();
    }
  }

  void flushBins() {
    if (!file_ || writeError_) return;
    const uint16_t upto = core_.count();
    while (writtenBins_ < upto) {
      const Bin& b = core_.bin(writtenBins_);
      if (file_.write(reinterpret_cast<const uint8_t*>(&b), sizeof(b)) != sizeof(b)) {
        writeError_ = true;
        file_.close();
        return;
      }
      ++writtenBins_;
      if ((writtenBins_ % 5) == 0) file_.flush(); // checkpoint every 5 seconds
    }
  }

  void finalizeFile() {
    if (!file_) return;
    file_.flush();
    file_.close();
    if (writeError_ || !storageReady_) return;
    // Accidental brief tap without enough samples must not destroy prior ride.
    if (core_.count() == 0) { SPIFFS.remove(activePath()); return; }
    // Preserve previous complete ride if the new file cannot be committed.
    SPIFFS.remove(recoveredPath());
    if (SPIFFS.exists(lastPath()) && !SPIFFS.rename(lastPath(), recoveredPath())) {
      writeError_ = true; return;
    }
    if (!SPIFFS.rename(activePath(), lastPath())) {
      // Restore previous ride when the final rename fails.
      if (SPIFFS.exists(recoveredPath())) SPIFFS.rename(recoveredPath(), lastPath());
      writeError_ = true;
    }
  }

  bool load(const char* path) {
    File f = SPIFFS.open(path, FILE_READ);
    if (!f) return false;
    uint8_t header[8] = {};
    if (f.read(header, 8) != 8) { f.close(); return false; }
    for (uint8_t j = 0; j < 8; ++j) {
      if (header[j] != magic()[j]) { f.close(); return false; }
    }
    core_.reset();
    Bin b;
    while (f.read(reinterpret_cast<uint8_t*>(&b), sizeof(b)) == sizeof(b)) {
      if (!core_.addLoadedBin(b)) break;
    }
    f.close();
    return core_.count() > 0;
  }

  Core core_;
  File file_;
  uint16_t writtenBins_ = 0;
  uint32_t nextSample_ = 0, readErrors_ = 0;
  uint8_t screen_ = 0;
  bool storageReady_ = false, writeError_ = false;
};
} // namespace trailecho
