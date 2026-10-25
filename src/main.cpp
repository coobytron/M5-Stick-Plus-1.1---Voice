// M5StickC Plus 1.1 voice translator.
// Hold A to record, release to transcribe and translate. B picks the language.
// See docs/DESIGN.md.

#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>

#include <algorithm>

#include "cloud.h"
#include "config.h"
#include "recorder.h"
#include "secrets.h"
#include "ui.h"

namespace {

enum class Screen { Idle, Recording, Result, Message };

Screen screen = Screen::Idle;
Preferences prefs;
int language = 0;
bool wifi_shown = false;
uint32_t last_draw = 0;

// Last result, kept so B can re-translate without recording again.
String transcript;
String spoken_language;
String translation;
int scroll = 0;
int max_scroll = 0;

const Language& target() { return LANGUAGES[language]; }

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

bool waitForWifi() {
  uint32_t start = millis();
  while (!wifiConnected() && millis() - start < WIFI_CONNECT_TIMEOUT_MS) delay(100);
  return wifiConnected();
}

void showIdle() {
  screen = Screen::Idle;
  wifi_shown = wifiConnected();
  ui::idle(target(), wifi_shown);
}

void showMessage(const char* title, const String& detail) {
  screen = Screen::Message;
  ui::message(target(), title, detail);
}

void showResult() {
  screen = Screen::Result;
  max_scroll = ui::result(target(), translation, transcript, spoken_language, scroll);
}

void translateTranscript() {
  ui::busy(target(), "Translating...");
  cloud::Result r = cloud::translate(transcript, target().name);
  if (!r.ok) {
    showMessage("Translation failed", r.text);
    return;
  }
  translation = r.text;
  scroll = 0;
  showResult();
}

void process(uint32_t clip_ms) {
  if (clip_ms < MIN_RECORD_MS) {
    showMessage("Too short", "Hold A while you speak.");
    return;
  }
  if (!waitForWifi()) {
    showMessage("No WiFi", String("Could not join ") + WIFI_SSID);
    return;
  }
  ui::busy(target(), "Transcribing...");
  cloud::Result heard = cloud::transcribe(RECORDING_PATH);
  if (!heard.ok) {
    showMessage("Speech-to-text failed", heard.text);
    return;
  }
  transcript = heard.text;
  spoken_language = heard.language;
  Serial.printf("Heard (%s): %s\n", spoken_language.c_str(), transcript.c_str());
  translateTranscript();
  if (screen == Screen::Result) Serial.printf("Translation: %s\n", translation.c_str());
}

void nextLanguage() {
  language = (language + 1) % LANGUAGE_COUNT;
  prefs.putInt("language", language);
  if (screen == Screen::Result && transcript.length()) {
    if (waitForWifi()) {
      translateTranscript();
    } else {
      showMessage("No WiFi", String("Could not join ") + WIFI_SSID);
    }
  } else {
    showIdle();
  }
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  prefs.begin("m5voice");
  language = prefs.getInt("language", 0);
  if (language < 0 || language >= LANGUAGE_COUNT) language = 0;

  ui::begin();
  if (!recorder::begin()) {
    showMessage("Storage or mic error", "Reflash the firmware, then try again.");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // TLS certificate checks need the real date.
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  showIdle();
}

void loop() {
  M5.update();

  if (recorder::active()) {
    bool room = recorder::poll();
    if (M5.BtnA.wasReleased() || !M5.BtnA.isPressed() || !room) {
      process(recorder::stop());
    } else if (millis() - last_draw > 100) {
      last_draw = millis();
      ui::recording(recorder::elapsedMs(), recorder::maxMs(), recorder::level());
    }
    return;
  }

  if (M5.BtnA.wasPressed()) {
    if (recorder::start()) {
      screen = Screen::Recording;
      ui::recording(0, recorder::maxMs(), 0);
    } else {
      showMessage("Can't record", "Storage is full or not mounted.");
    }
    return;
  }

  if (M5.BtnB.wasClicked()) {
    nextLanguage();
    return;
  }

  if (M5.BtnPWR.wasClicked()) {
    if (screen == Screen::Result) {
      scroll = scroll >= max_scroll ? 0 : std::min(scroll + 90, max_scroll);
      showResult();
    } else if (screen == Screen::Message) {
      showIdle();
    }
    return;
  }

  // Refresh the WiFi line on the idle screen when it changes.
  if (screen == Screen::Idle && wifiConnected() != wifi_shown) showIdle();
  delay(10);
}
