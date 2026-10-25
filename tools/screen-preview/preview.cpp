// Renders every screen in firmware/src/screen.cpp with sample data, without
// hardware. Run it with build.sh in this folder.
#include <time.h>

#include <string>

#include <Adafruit_GFX.h>

#include "screen.h"

const char* g_frameName = "frame";
static std::string g_outDir = ".";

void saveFrame(const char* name, GFXcanvas1& c) {
  std::string path = g_outDir + "/" + name + ".pgm";
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P5\n%d %d\n255\n", c.width(), c.height());
  for (int y = 0; y < c.height(); y++)
    for (int x = 0; x < c.width(); x++) fputc(c.getPixel(x, y) ? 255 : 0, f);
  fclose(f);
}

// The firmware's notes.cpp needs the SD card; only title() is needed here.
namespace notes {
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

static void frame(const char* name) { g_frameName = name; }

int main(int argc, char** argv) {
  if (argc > 1) g_outDir = argv[1];
  setenv("TZ", "UTC0", 1);
  tzset();
  // A fixed "now" can't be injected into time(), so dates on the sample notes
  // are fixed and the status-bar clock shows the real time of the run.
  const uint32_t ts = 1745662440;  // 2025-04-26 10:14 UTC

  Note meeting;
  meeting.seq = 12;
  meeting.ts = ts;
  meeting.seconds = 754;
  meeting.sent = true;
  meeting.text = "Meeting with Sam about the launch plan and who owns the beta list";
  Note fresh;
  fresh.seq = 13;
  fresh.ts = ts + 600;
  fresh.seconds = 41;
  Note idea;
  idea.seq = 11;
  idea.ts = ts - 86400;
  idea.seconds = 18;
  idea.sent = true;
  idea.text = "Idea for the clip a magnet instead of a spring";

  std::vector<uint8_t> levels;
  for (int i = 0; i < 28; i++) levels.push_back((uint8_t)(50 + 45 * sin(i * 0.9) * sin(i * 0.23)));

  screen::begin(false);
  frame("1-idle");
  screen::idle(78, 0);
  frame("2-idle-waiting");
  screen::idle(78, 3);
  frame("3-recording");
  screen::recording(12 * 60 + 34, levels, 78);
  frame("4-saved-sending");
  screen::saved(fresh, "Transcribing...", 78);
  frame("5-saved-titled");
  screen::saved(meeting, "Synced", 78);
  std::vector<Note> list = {fresh, meeting, idea};
  frame("6-files");
  screen::files(list, 1, 78);
  frame("7-detail");
  screen::detail(meeting, 78);
  frame("8-message");
  screen::message("No card", "Insert a microSD card.");
  return 0;
}
