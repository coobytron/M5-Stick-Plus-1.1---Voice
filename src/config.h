// Tunables. Secrets live in include/secrets.h (see secrets.example.h).
#pragma once

#include <stdint.h>

// Audio
constexpr uint32_t SAMPLE_RATE = 16000;     // Whisper resamples to 16 kHz anyway
constexpr uint32_t MAX_RECORD_SECONDS = 20;
constexpr uint32_t MIN_RECORD_MS = 500;     // shorter clips are ignored
constexpr const char* RECORDING_PATH = "/rec.wav";

// Speech-to-text (OpenAI)
constexpr const char* STT_HOST = "api.openai.com";
constexpr const char* STT_PATH = "/v1/audio/transcriptions";
constexpr const char* STT_MODEL = "whisper-1";

// Translation (Claude)
constexpr const char* CLAUDE_URL = "https://api.anthropic.com/v1/messages";
constexpr const char* CLAUDE_MODEL = "claude-opus-5-5";
constexpr const char* CLAUDE_VERSION = "2026-10-21";
constexpr const char* CLAUDE_EFFORT = "low";  // short phrases: no need to think hard

// Network
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint16_t HTTP_TIMEOUT_MS = 30000;

// Target languages, cycled with button B. `name` goes into the prompt as-is.
// Scripts outside the Japanese and Chinese efonts (Korean, Arabic, Thai, ...)
// would render as blanks, so they are left out for now.
struct Language {
  const char* name;
  const char* label;  // short label for the status bar
  bool chinese_font;
};

constexpr Language LANGUAGES[] = {
    {"Spanish", "ES", false},
    {"English", "EN", false},
    {"French", "FR", false},
    {"German", "DE", false},
    {"Italian", "IT", false},
    {"Portuguese", "PT", false},
    {"Japanese", "JA", false},
    {"Simplified Chinese", "ZH", true},
};
constexpr int LANGUAGE_COUNT = sizeof(LANGUAGES) / sizeof(LANGUAGES[0]);
