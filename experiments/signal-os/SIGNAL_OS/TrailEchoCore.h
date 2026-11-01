#pragma once
// Trail Echo's hardware-neutral motion processing. Host-testable with C++11.
#include <stdint.h>
#include <math.h>

namespace trailecho {

// Compact on-flash, one-second terrain signature. 4 bytes per second.
struct Bin {
  uint16_t rmsMg;       // RMS high-pass acceleration, 1/1000 g
  uint8_t peak10Mg;     // highest high-pass acceleration, 1/100 g (clamped)
  uint8_t impactCount;  // qualified impact peaks in the second (clamped)
};
static_assert(sizeof(Bin) == 4, "Trail Echo file format requires 4-byte bins");

class Core {
 public:
  static const uint16_t kCapacity = 7200;      // 2 hours maximum, 28.8 KB RAM
  static const uint32_t kWindowMs = 1000;
  static const uint32_t kImpactRefractoryMs = 180;

  Core() { reset(); }

  void reset() {
    count_ = 0;
    recording_ = false;
    complete_ = false;
    seenGravity_ = false;
    gx_ = gy_ = gz_ = 0;
    nextWindow_ = 0;
    sumSq_ = 0;
    samples_ = 0;
    maxDynamicG_ = 0;
    impacts_ = 0;
    lastImpactMs_ = 0;
    hadImpact_ = false;
    liveMg_ = 0;
  }

  void start(uint32_t nowMs) {
    reset();
    recording_ = true;
    nextWindow_ = nowMs + kWindowMs;
  }

  // Supply accelerometer components in g, not m/s^2. Call near 50 Hz.
  void sample(float ax, float ay, float az, uint32_t nowMs) {
    if (!recording_) return;
    advance(nowMs);  // Finalize completed second(s) before new samples.
    if (!recording_) return;
    if (!isfinite(ax) || !isfinite(ay) || !isfinite(az)) return;
    if (!seenGravity_) {
      gx_ = ax; gy_ = ay; gz_ = az;
      seenGravity_ = true;
      return;  // first sample sets initial orientation
    }
    // Slow gravity estimate; removes posture / slope but retains trail vibration.
    const float alpha = 0.02f;
    gx_ += alpha * (ax - gx_);
    gy_ += alpha * (ay - gy_);
    gz_ += alpha * (az - gz_);
    const float dx = ax - gx_, dy = ay - gy_, dz = az - gz_;
    float dynamicG = sqrtf(dx * dx + dy * dy + dz * dz);
    if (!isfinite(dynamicG)) return;
    if (dynamicG > 16.0f) dynamicG = 16.0f;
    liveMg_ = uint16_t(dynamicG * 1000.0f > 65535 ? 65535 : dynamicG * 1000.0f);
    sumSq_ += dynamicG * dynamicG;
    ++samples_;
    if (dynamicG > maxDynamicG_) maxDynamicG_ = dynamicG;
    // One distinct impact, not dozens of consecutive samples for the same bump.
    if (dynamicG > 0.70f &&
        (!hadImpact_ || uint32_t(nowMs - lastImpactMs_) >= kImpactRefractoryMs)) {
      if (impacts_ < 255) ++impacts_;
      lastImpactMs_ = nowMs;
      hadImpact_ = true;
    }
  }

  void advance(uint32_t nowMs) {
    while (recording_ && int32_t(nowMs - nextWindow_) >= 0) {
      pushWindow();
      nextWindow_ += kWindowMs;
    }
  }

  void stop(uint32_t nowMs) {
    if (!recording_) return;
    advance(nowMs);
    if (recording_ && samples_ > 2) pushWindow();
    recording_ = false;
    complete_ = true;
  }

  bool addLoadedBin(Bin b) {
    if (recording_ || count_ >= kCapacity) return false;
    bins_[count_++] = b;
    complete_ = true;
    return true;
  }

  bool recording() const { return recording_; }
  bool complete() const { return complete_; }
  uint16_t count() const { return count_; }
  uint16_t liveMg() const { return liveMg_; }
  const Bin& bin(uint16_t i) const { return bins_[i]; } // caller checks bounds

  uint16_t maxRmsMg() const {
    uint16_t highest = 0;
    for (uint16_t i = 0; i < count_; ++i)
      if (bins_[i].rmsMg > highest) highest = bins_[i].rmsMg;
    return highest;
  }

  uint32_t totalImpacts() const {
    uint32_t n = 0;
    for (uint16_t i = 0; i < count_; ++i) n += bins_[i].impactCount;
    return n;
  }

  uint16_t averageRmsMg() const {
    if (!count_) return 0;
    uint32_t sum = 0;
    for (uint16_t i = 0; i < count_; ++i) sum += bins_[i].rmsMg;
    return uint16_t(sum / count_);
  }

 private:
  void pushWindow() {
    if (count_ >= kCapacity) { recording_ = false; complete_ = true; return; }
    const float rmsG = samples_ ? sqrtf(sumSq_ / float(samples_)) : 0.0f;
    const uint32_t rm = uint32_t(rmsG * 1000.0f + 0.5f);
    const uint32_t pk = uint32_t(maxDynamicG_ * 100.0f + 0.5f);
    Bin b = {uint16_t(rm > 65535 ? 65535 : rm),
             uint8_t(pk > 255 ? 255 : pk), impacts_};
    bins_[count_++] = b;
    sumSq_ = 0;
    maxDynamicG_ = 0;
    samples_ = 0;
    impacts_ = 0;
    if (count_ >= kCapacity) { recording_ = false; complete_ = true; }
  }

  Bin bins_[kCapacity];
  uint16_t count_;
  bool recording_, complete_, seenGravity_, hadImpact_;
  float gx_, gy_, gz_;
  uint32_t nextWindow_, lastImpactMs_;
  float sumSq_, maxDynamicG_;
  uint16_t samples_, liveMg_;
  uint8_t impacts_;
};
} // namespace trailecho
