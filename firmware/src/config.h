// Pins and tunables for the pocket voice recorder. Wiring is in docs/DESIGN.md.
#pragma once

#include <Arduino.h>

// --- XIAO ESP32-S3 Sense, fixed by the board -------------------------------
constexpr int kMicClkPin = 42;   // on-board PDM mic (Sense expansion board)
constexpr int kMicDataPin = 41;
constexpr int kSdCsPin = 21;     // on-board microSD; shares GPIO21 with the user LED
constexpr int kSpiSckPin = 7;    // D8, shared by microSD and e-ink
constexpr int kSpiMisoPin = 8;   // D9
constexpr int kSpiMosiPin = 9;   // D10

// --- Wired by you -----------------------------------------------------------
constexpr int kEpdCsPin = 1;     // D0
constexpr int kEpdDcPin = 2;     // D1
constexpr int kEpdRstPin = 3;    // D2
constexpr int kEpdBusyPin = 4;   // D3
constexpr int kButtonPin = 5;    // D4, click button to GND (must be an RTC GPIO to wake from sleep)
constexpr int kBatteryPin = 6;   // D5, battery + through a 1:2 divider (two 220k resistors)
constexpr int kLedPin = 43;      // D6, orange status LED through 330R to GND (on while recording)

// --- Audio ------------------------------------------------------------------
constexpr uint32_t kSampleRate = 16000;  // what Whisper wants; 32 KB/s on the card
// Not tuned on hardware yet. Raise if transcripts miss soft speech, lower if
// loud speech distorts (listen with the receiver's --keep-audio).
constexpr int kMicGain = 4;
constexpr uint32_t kMaxRecordSeconds = 30 * 60;
// Covers SD-card write stalls (~2 s of audio).
constexpr size_t kAudioBufferBytes = 64 * 1024;

// --- Sync -------------------------------------------------------------------
constexpr uint32_t kWifiTimeoutMs = 12000;
constexpr uint32_t kTranscriptWaitMs = 90000;  // how long to wait for the Mac's transcript
constexpr uint32_t kRetryMinutes = 30;         // wake to retry while recordings are waiting

// --- Power ------------------------------------------------------------------
constexpr int kBatteryEmptyMv = 3400;  // rough LiPo curve; stop recording below this
constexpr int kBatteryFullMv = 4150;
constexpr uint32_t kDebounceMs = 60;

// --- Button -----------------------------------------------------------------
constexpr uint32_t kLongPressMs = 700;   // hold from asleep: open Files instead of recording
constexpr uint32_t kFilesIdleMs = 20000; // Files closes itself after this long untouched
constexpr uint32_t kSavedShowMs = 2500;  // how long "Saved" stays before the idle screen
