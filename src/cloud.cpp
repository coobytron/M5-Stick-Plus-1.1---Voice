#include "cloud.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

#include <algorithm>

#include "ca_roots.h"
#include "config.h"
#include "secrets.h"

namespace cloud {
namespace {

constexpr const char* BOUNDARY = "m5voice-7f3a9c2e";

// Streams a multipart body (form fields, then the WAV from flash, then the
// closing boundary) so the clip never has to fit in RAM.
class MultipartStream : public Stream {
 public:
  MultipartStream(const String& head, File& file, const String& tail)
      : head_(head), file_(file), tail_(tail) {}

  size_t size() const { return head_.length() + file_.size() + tail_.length(); }

  int available() override { return size() - pos_; }

  int read() override {
    char c;
    return readBytes(&c, 1) == 1 ? static_cast<uint8_t>(c) : -1;
  }

  int peek() override { return -1; }

  size_t readBytes(char* buffer, size_t length) override {
    size_t done = 0;
    while (done < length && pos_ < size()) {
      size_t n;
      if (pos_ < head_.length()) {
        n = std::min(length - done, head_.length() - pos_);
        memcpy(buffer + done, head_.c_str() + pos_, n);
      } else if (pos_ < head_.length() + file_.size()) {
        n = file_.read(reinterpret_cast<uint8_t*>(buffer + done), length - done);
        if (n == 0) break;
      } else {
        size_t t = pos_ - head_.length() - file_.size();
        n = std::min(length - done, tail_.length() - t);
        memcpy(buffer + done, tail_.c_str() + t, n);
      }
      done += n;
      pos_ += n;
    }
    return done;
  }

  size_t write(uint8_t) override { return 0; }

 private:
  const String& head_;
  File& file_;
  const String& tail_;
  size_t pos_ = 0;
};

String formField(const char* name, const char* value) {
  return String("--") + BOUNDARY + "\r\nContent-Disposition: form-data; name=\"" + name +
         "\"\r\n\r\n" + value + "\r\n";
}

Result fail(const String& message) {
  Result r;
  r.text = message;
  return r;
}

Result httpError(int code, const String& body) {
  Result r;
  if (code < 0) {
    r.text = String("Network error: ") + HTTPClient::errorToString(code);
    return r;
  }
  JsonDocument filter;
  filter["error"]["message"] = true;
  JsonDocument doc;
  deserializeJson(doc, body, DeserializationOption::Filter(filter));
  const char* message = doc["error"]["message"] | "";
  r.text = String("HTTP ") + code + (strlen(message) ? String(": ") + message : String(""));
  return r;
}

}  // namespace

Result transcribe(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) return fail("No recording found");

  String head = formField("model", STT_MODEL) + formField("response_format", "verbose_json") +
                "--" + BOUNDARY +
                "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"rec.wav\"\r\n"
                "Content-Type: audio/wav\r\n\r\n";
  String tail = String("\r\n--") + BOUNDARY + "--\r\n";
  MultipartStream body(head, file, tail);

  WiFiClientSecure client;
  client.setCACert(CA_ROOTS);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(client, String("https://") + STT_HOST + STT_PATH);
  http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
  http.addHeader("Content-Type", String("multipart/form-data; boundary=") + BOUNDARY);
  int code = http.sendRequest("POST", &body, body.size());
  String response = code > 0 ? http.getString() : String();
  http.end();
  file.close();

  if (code != 200) return httpError(code, response);

  JsonDocument filter;
  filter["text"] = true;
  filter["language"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, response, DeserializationOption::Filter(filter))) {
    return fail("Could not read transcript");
  }
  Result r;
  r.text = doc["text"] | "";
  r.text.trim();
  r.language = doc["language"] | "";
  r.ok = r.text.length() > 0;
  if (!r.ok) r.text = "Heard nothing";
  return r;
}

Result translate(const String& text, const char* target_language) {
  JsonDocument req;
  req["model"] = CLAUDE_MODEL;
  req["max_tokens"] = 2048;
  req["output_config"]["effort"] = CLAUDE_EFFORT;
  req["fallbacks"] = "default";
  req["system"] =
      String("You translate short spoken phrases for a pocket translator with a tiny screen. "
             "Translate the user's message into ") +
      target_language +
      ". Reply with only the translation: no quotes, notes, or romanization. "
      "If the message is already in that language, reply with it unchanged. "
      "The message is a speech-to-text transcript, so fix obvious recognition errors.";
  JsonObject msg = req["messages"].add<JsonObject>();
  msg["role"] = "user";
  msg["content"] = text;
  String payload;
  serializeJson(req, payload);

  WiFiClientSecure client;
  client.setCACert(CA_ROOTS);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(client, CLAUDE_URL);
  http.addHeader("content-type", "application/json");
  http.addHeader("x-api-key", ANTHROPIC_API_KEY);
  http.addHeader("anthropic-version", CLAUDE_VERSION);
  http.addHeader("anthropic-beta", "server-side-fallback-2026-07-01");
  int code = http.POST(payload);
  String response = code > 0 ? http.getString() : String();
  http.end();

  if (code != 200) return httpError(code, response);

  JsonDocument filter;
  filter["stop_reason"] = true;
  filter["content"][0]["type"] = true;
  filter["content"][0]["text"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, response, DeserializationOption::Filter(filter))) {
    return fail("Could not read translation");
  }
  if (doc["stop_reason"] == "refusal") return fail("Translation was declined");

  Result r;
  for (JsonObject block : doc["content"].as<JsonArray>()) {
    if (block["type"] == "text") r.text += block["text"] | "";
  }
  r.text.trim();
  r.ok = r.text.length() > 0;
  if (!r.ok) r.text = "Empty translation";
  return r;
}

}  // namespace cloud
