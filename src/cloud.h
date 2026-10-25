// HTTPS calls: speech-to-text (OpenAI) and translation (Claude).
#pragma once

#include <Arduino.h>

namespace cloud {

struct Result {
  bool ok = false;
  String text;      // transcript or translation; error message when !ok
  String language;  // spoken language detected by speech-to-text
};

// Uploads the WAV at `path` and returns what was said.
Result transcribe(const char* path);

// Translates `text` into `target_language` (a plain name such as "Spanish").
Result translate(const String& text, const char* target_language);

}  // namespace cloud
