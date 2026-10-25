// Just enough Arduino for screen.cpp and Adafruit GFX to build on a desktop.
#pragma once
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "Print.h"
#include "WString.h"
#include "pgmspace.h"

using std::max;
using std::min;
#define RTC_DATA_ATTR
#define IRAM_ATTR
#define HIGH 1
#define LOW 0
typedef bool boolean;
typedef uint8_t byte;
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define radians(deg) ((deg) * M_PI / 180.0)
inline void yield() {}
inline void delay(unsigned long) {}
