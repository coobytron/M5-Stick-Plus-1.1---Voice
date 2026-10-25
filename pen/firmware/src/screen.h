// Everything drawn on the e-ink window. E-ink keeps its image with no power,
// so whatever was drawn last stays visible while the pen sleeps.
#pragma once

#include <Arduino.h>

namespace screen {

// wokeFromSleep: the panel still shows our last image, so a fast partial
// refresh is safe. On a cold boot the first refresh is a full one.
void begin(bool wokeFromSleep);

// Resting screen: how many recordings wait for the Mac, battery, and the
// start of the last transcript (or a hint when there is none yet).
void home(const String& lastTranscript, int waiting, int batteryPercent);

// Recording timer and a level bar (0-100). Cheap partial refresh; call about
// once a second.
void recording(uint32_t seconds, int level);

// One headline and an optional line of detail, e.g. "Sending" / "3 to go".
void message(const char* title, const String& detail = "");

// Powers the panel's drivers down; the image stays.
void sleep();

}  // namespace screen
