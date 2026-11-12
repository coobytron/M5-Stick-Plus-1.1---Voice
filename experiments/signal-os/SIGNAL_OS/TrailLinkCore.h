#pragma once

// SIGNAL OS / Trail Link: transport-independent, deliberately small packet codec.
// This is NOT a safety beacon, authenticated protocol, positioning system, or radio driver.
// Byte format is specified below so an nRF24 or another transport can carry it later.
#include <stddef.h>
#include <stdint.h>

namespace traillink {

constexpr size_t kPacketBytes = 16;
constexpr uint8_t kVersion = 1;
constexpr uint8_t kUnknownBattery = 255;
constexpr uint32_t kDefaultFreshnessMs = 30000;

// Informational rider-to-rider presets only; never signal emergency dispatch.
enum class Kind : uint8_t { Ping = 1, Ok = 2, Wait = 3, Meet = 4 };

struct Packet {
  Kind kind;
  uint32_t sender;          // Locally generated pseudonymous device ID, not identity/authentication.
  uint16_t sequence;       // Increment per transmission; wraps naturally.
  uint16_t uptimeSeconds;  // Informational, wraps after ~18 h.
  uint8_t batteryPercent;
  constexpr Packet(Kind k = Kind::Ping, uint32_t id = 0,
                   uint16_t seq = 0, uint16_t up = 0,
                   uint8_t battery = kUnknownBattery)
      : kind(k), sender(id), sequence(seq), uptimeSeconds(up),
        batteryPercent(battery) {}
};

inline bool validKind(Kind kind) {
  return kind == Kind::Ping || kind == Kind::Ok ||
         kind == Kind::Wait || kind == Kind::Meet;
}
inline bool validBattery(uint8_t b) { return b <= 100 || b == kUnknownBattery; }

// CRC-16/CCITT-FALSE, polynomial 0x1021, initial value 0xFFFF.
// CRC detects accidental corruption. It does not authenticate or encrypt traffic.
inline uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xffff;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                           : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

inline void write16(uint8_t *out, uint16_t value) {
  out[0] = static_cast<uint8_t>(value);
  out[1] = static_cast<uint8_t>(value >> 8);
}
inline uint16_t read16(const uint8_t *in) {
  return static_cast<uint16_t>(in[0]) | (static_cast<uint16_t>(in[1]) << 8);
}
inline void write32(uint8_t *out, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(value >> (i * 8));
}
inline uint32_t read32(const uint8_t *in) {
  uint32_t result = 0;
  for (unsigned i = 0; i < 4; ++i) result |= static_cast<uint32_t>(in[i]) << (i * 8);
  return result;
}

//  0-1  "TL" magic
//  2    protocol version
//  3    Kind
//  4-7  sender (little-endian)
//  8-9  sequence (little-endian)
// 10-11 uptime seconds (little-endian)
// 12    battery percentage or 255 unknown
// 13    reserved = 0
// 14-15 CRC of bytes 0-13 (little-endian)
inline bool encode(const Packet &p, uint8_t *out, size_t outLen) {
  if (!out || outLen < kPacketBytes || !p.sender || !validKind(p.kind) ||
      !validBattery(p.batteryPercent)) return false;
  out[0] = 'T'; out[1] = 'L'; out[2] = kVersion;
  out[3] = static_cast<uint8_t>(p.kind);
  write32(out + 4, p.sender);
  write16(out + 8, p.sequence);
  write16(out + 10, p.uptimeSeconds);
  out[12] = p.batteryPercent;
  out[13] = 0;
  write16(out + 14, crc16(out, 14));
  return true;
}

inline bool decode(const uint8_t *in, size_t len, Packet &p) {
  if (!in || len != kPacketBytes || in[0] != 'T' || in[1] != 'L' ||
      in[2] != kVersion || in[13] != 0 || crc16(in, 14) != read16(in + 14))
    return false;
  const Packet candidate{static_cast<Kind>(in[3]), read32(in + 4),
                         read16(in + 8), read16(in + 10), in[12]};
  if (!candidate.sender || !validKind(candidate.kind) ||
      !validBattery(candidate.batteryPercent)) return false;
  p = candidate;
  return true;
}

// Tracks ONE previously configured peer. Enforce explicit peer filtering; this
// is only an accidental-cross-talk guard, NOT a security or spoofing defense.
class Tracker {
 public:
  explicit Tracker(uint32_t expectedPeer) : expectedPeer_(expectedPeer) {}
  bool receive(const Packet &packet, uint32_t receivedAtMs) {
    if (!expectedPeer_ || packet.sender != expectedPeer_ ||
        !validKind(packet.kind) || !validBattery(packet.batteryPercent)) return false;
    // Restart detection after a long silence: a peer might have rebooted and
    // reset its sequence counter. This also means stale packets are not secure.
    if (hasPacket_ && (uint32_t)(receivedAtMs - receivedAtMs_) < 60000U) {
      const uint16_t delta = static_cast<uint16_t>(packet.sequence - last_.sequence);
      if (delta == 0 || delta >= 0x8000) return false;
    }
    last_ = packet;
    receivedAtMs_ = receivedAtMs;
    ++receivedCount_;
    hasPacket_ = true;
    return true;
  }
  bool hasPacket() const { return hasPacket_; }
  bool fresh(uint32_t nowMs, uint32_t ageMs = kDefaultFreshnessMs) const {
    return hasPacket_ && static_cast<uint32_t>(nowMs - receivedAtMs_) <= ageMs;
  }
  uint32_t ageMs(uint32_t nowMs) const {
    return hasPacket_ ? static_cast<uint32_t>(nowMs - receivedAtMs_) : UINT32_MAX;
  }
  uint32_t receivedCount() const { return receivedCount_; }
  const Packet &last() const { return last_; }
 private:
  uint32_t expectedPeer_ = 0;
  uint32_t receivedAtMs_ = 0;
  uint32_t receivedCount_ = 0;
  Packet last_{};
  bool hasPacket_ = false;
};

}  // namespace traillink
