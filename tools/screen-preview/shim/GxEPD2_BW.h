// Stand-in for GxEPD2: draws into an Adafruit GFX canvas and saves each
// refresh as a PGM image instead of driving a panel. A set canvas bit is
// white (GxEPD_WHITE is non-zero), a clear one black.
#pragma once
#include <Adafruit_GFX.h>

#define GxEPD_WHITE 0xFFFF
#define GxEPD_BLACK 0x0000

struct GxEPD2_213_B74 {
  static const uint16_t WIDTH_VISIBLE = 122;
  static const uint16_t HEIGHT = 250;
  GxEPD2_213_B74(int, int, int, int) {}
};

// Set by the preview program before each frame.
extern const char* g_frameName;
void saveFrame(const char* name, GFXcanvas1& canvas);

template <typename Panel, int page_height>
class GxEPD2_BW : public GFXcanvas1 {
 public:
  explicit GxEPD2_BW(Panel) : GFXcanvas1(Panel::WIDTH_VISIBLE, Panel::HEIGHT) {}
  void init(uint32_t, bool, uint16_t, bool) {}
  void setFullWindow() {}
  void setPartialWindow(int16_t, int16_t, int16_t, int16_t) {}
  void display(bool) { saveFrame(g_frameName, *this); }
  void hibernate() {}
};
