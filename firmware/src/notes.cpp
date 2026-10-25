#include "notes.h"

#include <SD.h>

namespace {

constexpr const char* kPath = "/notes.txt";
constexpr const char* kTmpPath = "/notes.tmp";
constexpr size_t kMaxNotes = 50;
constexpr size_t kMaxTextChars = 160;  // enough for the Files detail view

// One line per note: seq|ts|seconds|sent|text. Text never holds '|' or newlines.
String clean(String s) {
  s.replace("|", "/");
  s.replace("\n", " ");
  s.replace("\r", " ");
  s.trim();
  if (s.length() > kMaxTextChars) s = s.substring(0, kMaxTextChars);
  return s;
}

bool parse(const String& line, Note& n) {
  int a = line.indexOf('|');
  int b = line.indexOf('|', a + 1);
  int c = line.indexOf('|', b + 1);
  int d = line.indexOf('|', c + 1);
  if (a < 0 || b < 0 || c < 0 || d < 0) return false;
  n.seq = line.substring(0, a).toInt();
  n.ts = strtoul(line.substring(a + 1, b).c_str(), nullptr, 10);
  n.seconds = line.substring(b + 1, c).toInt();
  n.sent = line.substring(c + 1, d) == "1";
  n.text = line.substring(d + 1);
  n.text.trim();
  return n.seq > 0;
}

}  // namespace

namespace notes {

std::vector<Note> load() {
  std::vector<Note> all;
  File f = SD.open(kPath);
  if (!f) return all;
  while (f.available() && all.size() < kMaxNotes) {
    Note n;
    if (parse(f.readStringUntil('\n'), n)) all.push_back(n);
  }
  return all;
}

void save(const std::vector<Note>& all) {
  // Write a new file, then swap, so a dead battery mid-write can't lose the list.
  File f = SD.open(kTmpPath, FILE_WRITE);
  if (!f) return;
  for (size_t i = 0; i < all.size() && i < kMaxNotes; i++) {
    const Note& n = all[i];
    f.printf("%u|%lu|%u|%d|%s\n", (unsigned)n.seq, (unsigned long)n.ts, (unsigned)n.seconds, n.sent ? 1 : 0,
             clean(n.text).c_str());
  }
  f.close();
  SD.remove(kPath);
  SD.rename(kTmpPath, kPath);
}

void add(const Note& n) {
  auto all = load();
  all.insert(all.begin(), n);
  save(all);
}

void markSent(uint32_t seq, const String& text) {
  auto all = load();
  for (auto& n : all) {
    if (n.seq != seq) continue;
    n.sent = true;
    if (text.length() > 0) n.text = text;
  }
  save(all);
}

String title(const Note& n, int words) {
  if (n.text.length() == 0) return String("Note ") + n.seq;
  int end = 0;
  for (int w = 0; w < words; w++) {
    int space = n.text.indexOf(' ', end + 1);
    if (space < 0) return n.text;
    end = space;
  }
  return n.text.substring(0, end);
}

}  // namespace notes
