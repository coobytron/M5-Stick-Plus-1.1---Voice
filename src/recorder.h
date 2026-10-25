// Records the PDM mic to a WAV file on LittleFS.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace recorder {

// Mounts LittleFS (formatting it on first boot) and starts the mic.
bool begin();

// Opens RECORDING_PATH and starts capturing. Returns false if the file
// cannot be opened or there is no room.
bool start();

// Call every loop while recording. Returns false once the clip is full.
bool poll();

// Stops capturing, writes the final header, and returns the clip length in ms.
uint32_t stop();

bool active();
uint32_t elapsedMs();
uint32_t maxMs();

// Peak level of the last captured buffer, 0..1, for the on-screen meter.
float level();

}  // namespace recorder
