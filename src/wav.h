// Minimal PCM WAV header (44 bytes, little-endian, which the ESP32 is).
#pragma once

#include <stdint.h>

struct __attribute__((packed)) WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t riff_size = 36;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmt_size = 16;
  uint16_t format = 1;  // PCM
  uint16_t channels = 1;
  uint32_t sample_rate = 16000;
  uint32_t byte_rate = 32000;
  uint16_t block_align = 2;
  uint16_t bits_per_sample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t data_size = 0;

  WavHeader(uint32_t rate, uint32_t pcm_bytes)
      : riff_size(36 + pcm_bytes),
        sample_rate(rate),
        byte_rate(rate * 2),
        data_size(pcm_bytes) {}
};
static_assert(sizeof(WavHeader) == 44, "WAV header must be 44 bytes");
