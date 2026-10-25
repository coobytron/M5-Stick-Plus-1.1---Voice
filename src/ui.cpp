#include "ui.h"

#include <M5Unified.h>

#include <algorithm>

namespace ui {
namespace {

// Draw into an off-screen canvas, then push it in one go to avoid flicker.
// 8-bit color keeps it at ~31 KB, leaving heap for TLS.
constexpr int MARGIN = 4;
constexpr int WIDTH = 240 - 2 * MARGIN;
constexpr int HEIGHT = 135;
constexpr int BAR = 20;  // status bar height

M5Canvas canvas(&M5.Display);

const lgfx::IFont* bodyFont(const Language& target) {
  return target.chinese_font ? &fonts::efontCN_14 : &fonts::efontJA_14;
}

void statusBar(const Language& target, uint16_t bg = TFT_NAVY) {
  canvas.fillRect(0, 0, WIDTH, BAR, bg);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(TFT_WHITE, bg);
  canvas.setTextDatum(middle_left);
  canvas.drawString(String("To: ") + target.name, 2, BAR / 2);
  canvas.setTextDatum(middle_right);
  int battery = M5.Power.getBatteryLevel();
  if (battery >= 0) canvas.drawString(String(battery) + "%", WIDTH - 2, BAR / 2);
  canvas.setTextDatum(top_left);
}

void push() { canvas.pushSprite(MARGIN, 0); }

// Prints `text` with breaks between words. Words wider than a line, and
// scripts without spaces (Japanese, Chinese), fall back to breaking anywhere.
void printWrapped(const String& text) {
  int start = 0;
  int len = text.length();
  while (start <= len) {
    int end = start;
    while (end < len && text[end] != ' ' && text[end] != '\n') ++end;
    String word = text.substring(start, end);
    int w = canvas.textWidth(word.c_str());
    if (canvas.getCursorX() > 0 && canvas.getCursorX() + w > WIDTH) canvas.println();
    canvas.print(word);
    if (end < len && text[end] == '\n') {
      canvas.println();
    } else if (end < len && canvas.getCursorX() > 0) {
      canvas.print(' ');
    }
    start = end + 1;
  }
}

}  // namespace

void begin() {
  M5.Display.setRotation(1);
  M5.Display.setBrightness(128);
  M5.Display.fillScreen(TFT_BLACK);
  canvas.setColorDepth(8);
  canvas.createSprite(WIDTH, HEIGHT);
  canvas.setTextWrap(true, false);
}

void idle(const Language& target, bool wifi_connected) {
  canvas.fillScreen(TFT_BLACK);
  statusBar(target);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(middle_center);
  canvas.drawString("Hold A to talk", WIDTH / 2, 58);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString("B: change language", WIDTH / 2, 90);
  canvas.setTextColor(wifi_connected ? TFT_GREEN : TFT_ORANGE);
  canvas.drawString(wifi_connected ? "WiFi connected" : "Connecting to WiFi...", WIDTH / 2, 115);
  canvas.setTextDatum(top_left);
  push();
}

void recording(uint32_t elapsed_ms, uint32_t max_ms, float level) {
  canvas.fillScreen(TFT_BLACK);
  canvas.fillRect(0, 0, WIDTH, BAR, TFT_RED);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(TFT_WHITE, TFT_RED);
  canvas.setTextDatum(middle_center);
  canvas.drawString("REC  release A to stop", WIDTH / 2, BAR / 2);

  canvas.setFont(&fonts::FreeSansBold18pt7b);
  canvas.setTextColor(TFT_WHITE);
  char secs[16];
  snprintf(secs, sizeof(secs), "%.1fs", elapsed_ms / 1000.0f);
  canvas.drawString(secs, WIDTH / 2, 58);
  canvas.setTextDatum(top_left);

  // Input level, then time used.
  int level_w = static_cast<int>(std::min(1.0f, level * 4) * WIDTH);
  canvas.fillRect(0, 92, level_w, 12, TFT_GREEN);
  canvas.drawRect(0, 92, WIDTH, 12, TFT_DARKGREY);
  int time_w = max_ms ? static_cast<int>(static_cast<uint64_t>(WIDTH) * elapsed_ms / max_ms) : 0;
  canvas.fillRect(0, 114, time_w, 8, TFT_RED);
  canvas.drawRect(0, 114, WIDTH, 8, TFT_DARKGREY);
  push();
}

void busy(const Language& target, const char* step) {
  message(target, step, "");
}

void message(const Language& target, const char* title, const String& detail) {
  canvas.fillScreen(TFT_BLACK);
  statusBar(target);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(middle_center);
  canvas.drawString(title, WIDTH / 2, detail.length() ? 42 : 72);
  canvas.setTextDatum(top_left);
  if (detail.length()) {
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setCursor(0, 62);
    printWrapped(detail);
  }
  push();
}

int result(const Language& target, const String& translation, const String& transcript,
           const String& spoken_language, int scroll) {
  canvas.fillScreen(TFT_BLACK);
  canvas.setFont(bodyFont(target));
  canvas.setTextColor(TFT_WHITE);
  int top = BAR + 4;
  canvas.setCursor(0, top - scroll);
  printWrapped(translation);
  canvas.println();
  canvas.println();
  canvas.setTextColor(TFT_DARKGREY);
  String heard = "Heard";
  if (spoken_language.length()) heard += " (" + spoken_language + ")";
  printWrapped(heard + ": " + transcript);
  int content_bottom = canvas.getCursorY() + canvas.fontHeight() + scroll;

  // Drawn last so scrolled text slides under it.
  statusBar(target);
  push();
  return std::max(0, content_bottom - HEIGHT);
}

}  // namespace ui
