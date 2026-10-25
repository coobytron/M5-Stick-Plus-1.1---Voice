#include "screen.h"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <GxEPD2_BW.h>

#include "config.h"

#if defined(PEN_DISPLAY_097)
#include <GxEPD2_097_T61.h>
using Panel = GxEPD2_097_T61;
#elif defined(PEN_DISPLAY_213)
using Panel = GxEPD2_213_B74;
#else
#error "Build with -DPEN_DISPLAY_097 or -DPEN_DISPLAY_213 (see platformio.ini)"
#endif

namespace {

// Full-height buffer: 2.3 KB for the 0.97" panel, 4 KB for the 2.13".
GxEPD2_BW<Panel, Panel::HEIGHT> display(Panel(kEpdCsPin, kEpdDcPin, kEpdRstPin, kEpdBusyPin));

// Partial refreshes are fast but leave faint ghosts; every few home screens
// get a full (flashing) refresh to wipe them. Kept across deep sleep.
constexpr int kFullRefreshEvery = 6;
RTC_DATA_ATTR int partialsSinceFull = 0;

constexpr int kMargin = 4;

void beginFrame(bool full) {
  if (full || partialsSinceFull >= kFullRefreshEvery) {
    display.setFullWindow();
    partialsSinceFull = 0;
  } else {
    display.setPartialWindow(0, 0, display.width(), display.height());
    partialsSinceFull++;
  }
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
}

int textWidth(const String& s) {
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x, &y, &w, &h);
  return w;
}

// Word-wraps text into at most maxLines lines starting at baseline y, adding
// "..." when it doesn't fit. Returns the baseline after the last line.
int drawWrapped(String text, int y, int maxLines, int lineHeight) {
  int width = display.width() - 2 * kMargin;
  text.trim();
  for (int line = 0; line < maxLines && text.length() > 0; line++) {
    bool last = line == maxLines - 1;
    // Longest run of whole words that fits.
    int end = 0;
    int next = 0;
    while (next < (int)text.length()) {
      int space = text.indexOf(' ', next);
      int candidate = space < 0 ? text.length() : space;
      if (textWidth(text.substring(0, candidate)) > width) break;
      end = candidate;
      next = candidate + 1;
    }
    if (end == 0) end = min<int>(text.length(), 1);  // one very long word
    String piece = text.substring(0, end);
    String rest = text.substring(end);
    rest.trim();
    if (last && rest.length() > 0) {
      while (piece.length() > 0 && textWidth(piece + "...") > width) piece.remove(piece.length() - 1);
      piece += "...";
    }
    display.setCursor(kMargin, y);
    display.print(piece);
    y += lineHeight;
    text = rest;
  }
  return y;
}

int clampPercent(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

}  // namespace

namespace screen {

void begin(bool wokeFromSleep) {
  // initial=false tells GxEPD2 the panel already shows something we drew.
  display.init(0, !wokeFromSleep, 10, false);
  display.setRotation(1);  // landscape: the long side runs along the pen
  display.setTextWrap(false);
}

void home(const String& lastTranscript, int waiting, int batteryPercent) {
  beginFrame(false);
  display.setFont(&FreeSans9pt7b);

  // Status line: sync state on the left, battery on the right.
  String status = waiting == 0 ? String("Synced") : String(waiting) + " waiting";
  display.setCursor(kMargin, 14);
  display.print(status);
  String battery = String(clampPercent(batteryPercent)) + "%";
  display.setCursor(display.width() - kMargin - textWidth(battery), 14);
  display.print(battery);
  display.drawFastHLine(kMargin, 20, display.width() - 2 * kMargin, GxEPD_BLACK);

  int lines = (display.height() - 26) / 20;
  if (lastTranscript.length() > 0) {
    drawWrapped(lastTranscript, 38, lines, 20);
  } else {
    drawWrapped("Click the top to record.", 38, lines, 20);
  }
  display.display(true);
}

void recording(uint32_t seconds, int level) {
  beginFrame(false);
  // Recording screens are throwaway: don't count them toward the full refresh.
  partialsSinceFull = max(0, partialsSinceFull - 1);

  display.fillCircle(kMargin + 7, 22, 7, GxEPD_BLACK);
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(kMargin + 20, 30);
  display.print("REC");

  char clock[8];
  snprintf(clock, sizeof(clock), "%u:%02u", (unsigned)(seconds / 60), (unsigned)(seconds % 60));
  display.setFont(&FreeSansBold18pt7b);
  display.setCursor(kMargin, display.height() - 22);
  display.print(clock);

  int barW = display.width() - 2 * kMargin;
  int barY = display.height() - 12;
  display.drawRect(kMargin, barY, barW, 8, GxEPD_BLACK);
  display.fillRect(kMargin, barY, barW * clampPercent(level) / 100, 8, GxEPD_BLACK);
  display.display(true);
}

void message(const char* title, const String& detail) {
  beginFrame(false);
  partialsSinceFull = max(0, partialsSinceFull - 1);
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(kMargin, 30);
  display.print(title);
  if (detail.length() > 0) {
    display.setFont(&FreeSans9pt7b);
    drawWrapped(detail, 56, 2, 20);
  }
  display.display(true);
}

void sleep() { display.hibernate(); }

}  // namespace screen
