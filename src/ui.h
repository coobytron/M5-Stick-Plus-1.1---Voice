// All drawing. Landscape, 240 x 135.
#pragma once

#include <Arduino.h>

#include "config.h"

namespace ui {

void begin();

void idle(const Language& target, bool wifi_connected);
void recording(uint32_t elapsed_ms, uint32_t max_ms, float level);
void busy(const Language& target, const char* step);
void message(const Language& target, const char* title, const String& detail);

// Draws the translation and transcript, scrolled down by `scroll` pixels.
// Returns the largest useful scroll value, so the caller can wrap around.
int result(const Language& target, const String& translation, const String& transcript,
           const String& spoken_language, int scroll);

}  // namespace ui
