#include "screen.h"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <GxEPD2_BW.h>
#include <time.h>

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

// Full-height buffer: 2 KB for the 0.97" panel, 4 KB for the 2.13".
GxEPD2_BW<Panel, Panel::HEIGHT> display(Panel(kEpdCsPin, kEpdDcPin, kEpdRstPin, kEpdBusyPin));

// Partial refreshes are fast but leave faint ghosts; every few resting screens
// get a full (flashing) refresh to wipe them. Kept across deep sleep.
constexpr int kFullRefreshEvery = 6;
RTC_DATA_ATTR int partialsSinceFull = 0;

constexpr int kMargin = 3;
constexpr int kStatusH = 16;  // status bar: clock left, battery right
constexpr int kLine = 18;     // FreeSans 9pt line height
// Small print uses GFX's built-in 6 x 8 font, whose cursor is the top-left
// corner rather than the baseline.

bool clockIsSet() { return time(nullptr) > 1700000000; }

String formatTime(uint32_t ts, const char* fmt) {
  time_t t = ts;
  struct tm tm;
  localtime_r(&t, &tm);
  char buf[24];
  strftime(buf, sizeof(buf), fmt, &tm);
  return buf;
}

// resting: a screen that stays while asleep, counted toward the periodic
// ghost wipe. Screens that only last a moment always use a partial refresh.
void beginFrame(bool resting) {
  if (resting && partialsSinceFull >= kFullRefreshEvery) {
    display.setFullWindow();
    partialsSinceFull = 0;
  } else {
    display.setPartialWindow(0, 0, display.width(), display.height());
    if (resting) partialsSinceFull++;
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

void centered(const String& s, int y) {
  display.setCursor((display.width() - textWidth(s)) / 2, y);
  display.print(s);
}

// Cuts s down to fit width, ending in "..." when shortened.
String fit(String s, int width) {
  if (textWidth(s) <= width) return s;
  while (s.length() > 0 && textWidth(s + "...") > width) s.remove(s.length() - 1);
  return s + "...";
}

// Word-wraps text into at most maxLines lines from baseline y, centered or
// left-aligned. Returns the baseline after the last line.
int wrapped(String text, int y, int maxLines, bool center) {
  int width = display.width() - 2 * kMargin;
  text.trim();
  for (int line = 0; line < maxLines && text.length() > 0; line++) {
    int end = 0;
    int next = 0;
    while (next < (int)text.length()) {
      int space = text.indexOf(' ', next);
      int candidate = space < 0 ? text.length() : space;
      if (textWidth(text.substring(0, candidate)) > width) break;
      end = candidate;
      next = candidate + 1;
    }
    if (end == 0) end = min<int>(text.length(), 1);
    String piece = text.substring(0, end);
    String rest = text.substring(end);
    rest.trim();
    if (line == maxLines - 1 && rest.length() > 0) piece = fit(piece + " " + rest, width);
    if (center) {
      centered(piece, y);
    } else {
      display.setCursor(kMargin, y);
      display.print(piece);
    }
    y += kLine;
    text = rest;
  }
  return y;
}

int clampPercent(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

void battery(int percent) {
  const int w = 16, h = 8;
  int x = display.width() - kMargin - w - 2;
  int y = 4;
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  display.fillRect(x + w, y + 2, 2, h - 4, GxEPD_BLACK);  // terminal nub
  display.fillRect(x + 2, y + 2, (w - 4) * clampPercent(percent) / 100, h - 4, GxEPD_BLACK);
}

// Clock (when known and showClock) and battery across the top.
void statusBar(int batteryPercent, bool showClock) {
  if (showClock && clockIsSet()) {
    display.setFont(nullptr);
    display.setCursor(kMargin, 4);
    display.print(formatTime(time(nullptr), "%H:%M"));
  }
  battery(batteryPercent);
}

void checkMark(int cx, int cy, int r) {
  display.fillCircle(cx, cy, r, GxEPD_BLACK);
  // Two thick strokes in white.
  for (int d = -1; d <= 1; d++) {
    display.drawLine(cx - r / 2, cy + d, cx - r / 8, cy + r / 3 + d, GxEPD_WHITE);
    display.drawLine(cx - r / 8, cy + r / 3 + d, cx + r / 2, cy - r / 3 + d, GxEPD_WHITE);
  }
}

}  // namespace

namespace screen {

void begin(bool wokeFromSleep) {
  // initial=false tells GxEPD2 the panel already shows something we drew.
  display.init(0, !wokeFromSleep, 10, false);
  display.setRotation(0);  // portrait: the long side runs along the recorder
  display.setTextWrap(false);
}

void idle(int batteryPercent, int waiting) {
  beginFrame(true);
  statusBar(batteryPercent, false);
  display.setFont(&FreeSans9pt7b);
  int mid = display.height() / 2;
  centered("Press to", mid - 2);
  centered("Record", mid - 2 + kLine);
  if (waiting > 0) {
    display.setFont(nullptr);  // built-in 6 x 8 font for the small print
    centered(String(waiting) + " to sync", display.height() - 10);
  }
  display.display(true);
}

void recording(uint32_t seconds, const std::vector<uint8_t>& levels, int batteryPercent) {
  beginFrame(false);
  statusBar(batteryPercent, true);

  display.setFont(nullptr);
  String label = "Recording";
  int labelX = (display.width() - textWidth(label) + 9) / 2;
  display.fillCircle(labelX - 6, 45, 2, GxEPD_BLACK);
  display.setCursor(labelX, 42);
  display.print(label);

  char clock[12];
  snprintf(clock, sizeof(clock), "%02u:%02u:%02u", (unsigned)(seconds / 3600), (unsigned)(seconds / 60 % 60),
           (unsigned)(seconds % 60));
  display.setFont(&FreeSansBold9pt7b);
  centered(clock, 84);

  // Waveform: one 2 px bar per level, mirrored around a centre line.
  int cy = 124;
  int maxH = 22;
  int bars = min<int>(levels.size(), (display.width() - 2 * kMargin) / 3);
  int x0 = (display.width() - bars * 3) / 2;
  for (int i = 0; i < bars; i++) {
    int h = max(1, levels[levels.size() - bars + i] * maxH / 100);
    display.fillRect(x0 + i * 3, cy - h, 2, 2 * h, GxEPD_BLACK);
  }
  display.display(true);
}

void saved(const Note& note, const String& status, int batteryPercent) {
  beginFrame(false);
  statusBar(batteryPercent, true);
  checkMark(display.width() / 2, 40, 13);

  display.setFont(&FreeSans9pt7b);
  centered("Saved", 74);
  int y = wrapped(notes::title(note), 94, 2, true);
  if (note.ts > 0) {
    display.setFont(nullptr);
    centered(formatTime(note.ts, "%Y-%m-%d"), y - 6);
    centered(formatTime(note.ts, "%H:%M"), y + 6);
  }
  if (status.length() > 0) {
    display.setFont(nullptr);
    centered(status, display.height() - 10);
  }
  display.display(true);
}

void files(const std::vector<Note>& list, int selected, int batteryPercent) {
  beginFrame(false);
  statusBar(batteryPercent, true);
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(kMargin, kStatusH + 18);
  display.print("Files");

  // Rows: the notes, then "Back". Scrolls so the selection stays visible.
  const int rowH = 20;
  const int top = kStatusH + 26;
  int rows = list.size() + 1;
  int visible = (display.height() - top) / rowH;
  int first = max(0, min(selected - visible + 1, rows - visible));
  display.setFont(&FreeSans9pt7b);
  for (int r = first; r < rows && r < first + visible; r++) {
    int y = top + (r - first) * rowH;
    bool sel = r == selected;
    String label = r < (int)list.size() ? notes::title(list[r], 3) : String("Back");
    if (sel) display.fillRect(0, y, display.width(), rowH, GxEPD_BLACK);
    display.setTextColor(sel ? GxEPD_WHITE : GxEPD_BLACK);
    display.setCursor(kMargin + 2, y + 14);
    display.print(fit(label, display.width() - 2 * kMargin - 8));
    if (r < (int)list.size() && !list[r].sent) {
      // Small dot: still waiting for the Mac.
      display.fillCircle(display.width() - kMargin - 3, y + rowH / 2, 2, sel ? GxEPD_WHITE : GxEPD_BLACK);
    }
  }
  display.setTextColor(GxEPD_BLACK);
  display.display(true);
}

void detail(const Note& note, int batteryPercent) {
  beginFrame(false);
  statusBar(batteryPercent, true);
  display.setFont(&FreeSansBold9pt7b);
  int y = wrapped(notes::title(note, 3), kStatusH + 18, 2, false);

  // Date, then time and length, then sync state: each fits 88 px at 6 px a character.
  display.setFont(nullptr);
  y -= 8;
  if (note.ts > 0) {
    display.setCursor(kMargin, y);
    display.print(formatTime(note.ts, "%Y-%m-%d"));
    y += 10;
    display.setCursor(kMargin, y);
    display.print(formatTime(note.ts, "%H:%M"));
    display.print("  ");
  } else {
    display.setCursor(kMargin, y);
  }
  display.printf("%u:%02u", (unsigned)(note.seconds / 60), (unsigned)(note.seconds % 60));
  y += 10;
  display.setCursor(kMargin, y);
  display.print(note.sent ? "synced" : "not synced yet");

  // The rest of the transcript, after the words already shown as the title.
  display.setFont(&FreeSans9pt7b);
  String title = notes::title(note, 3);
  String body = note.text.length() > title.length() ? note.text.substring(title.length()) : String("");
  if (note.text.length() == 0) body = "No transcript yet.";
  int top = y + 26;
  wrapped(body, top, (display.height() - top) / kLine + 1, false);
  display.display(true);
}

void message(const char* title, const String& detail) {
  beginFrame(false);
  display.setFont(&FreeSansBold9pt7b);
  int y = wrapped(title, display.height() / 2 - 20, 2, true);
  if (detail.length() > 0) {
    display.setFont(&FreeSans9pt7b);
    wrapped(detail, y + 6, 4, true);
  }
  display.display(true);
}

void sleep() { display.hibernate(); }

}  // namespace screen
