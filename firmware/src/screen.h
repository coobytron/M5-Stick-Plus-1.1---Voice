// Everything drawn on the e-ink window, in portrait, following the MVP
// concept (docs/reference/concept-mvp.png). E-ink keeps its image with no
// power, so whatever was drawn last stays visible while the recorder sleeps.
#pragma once

#include <Arduino.h>

#include <vector>

#include "notes.h"

namespace screen {

// wokeFromSleep: the panel still shows our last image, so a fast partial
// refresh is safe. On a cold boot the first refresh is a full one.
void begin(bool wokeFromSleep);

// Resting screen: battery and "Press to Record". The clock is left off
// because it would go stale while asleep; waiting > 0 adds "3 to sync".
void idle(int batteryPercent, int waiting);

// Timer and a waveform of recent levels (each 0-100, oldest first).
void recording(uint32_t seconds, const std::vector<uint8_t>& levels, int batteryPercent);

// Check mark, "Saved", the note's title and when it was recorded. status is a
// short line at the bottom such as "Sending..." (empty for none).
void saved(const Note& note, const String& status, int batteryPercent);

// The Files list, newest first, with one row selected. The last row is "Back".
void files(const std::vector<Note>& list, int selected, int batteryPercent);

// One note: title, date, length and the start of its transcript.
void detail(const Note& note, int batteryPercent);

// A headline and a line of detail, for problems such as "No card".
void message(const char* title, const String& detail);

// Powers the panel's drivers down; the image stays.
void sleep();

}  // namespace screen
