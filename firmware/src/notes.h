// The list of recordings behind the Files screen, kept in /notes.txt on the
// microSD card. Audio lives separately in /rec until the Mac has it.
#pragma once

#include <Arduino.h>

#include <vector>

struct Note {
  uint32_t seq = 0;
  uint32_t ts = 0;       // unix time it was recorded, 0 if the clock wasn't set yet
  uint32_t seconds = 0;
  bool sent = false;     // the Mac has the audio
  String text;           // start of the transcript, empty until it comes back
};

namespace notes {

// Newest first. Keeps at most kMaxNotes.
std::vector<Note> load();
void save(const std::vector<Note>& all);

void add(const Note& n);
// Marks a note sent and, if text is not empty, stores its transcript.
void markSent(uint32_t seq, const String& text);

// "Meeting with Sam about" from a transcript; "Note 12" without one.
String title(const Note& n, int words = 4);

}  // namespace notes
