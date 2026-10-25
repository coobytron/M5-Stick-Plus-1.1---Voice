# Voice pen

A pen-shaped voice recorder with an e-ink window. Click the top, talk, click again. The pen saves the recording, sends it to your Mac over Wi-Fi, and the Mac transcribes it locally with Whisper, free and private. The first lines of the transcript come back to the e-ink window and stay there, with no power, until your next note.

![Front: e-ink window near the click button](cad/renders/front.png)

*OpenSCAD preview of the case, not a photo of a built pen.*

- **Electronics:** Seeed XIAO ESP32-S3 Sense (mic, microSD, battery charging) and a 0.97" Good Display e-ink panel.
- **Case:** one parametric OpenSCAD file, five printed parts: a seamless body, an inner sled, a click cap, a plunger and a clip.
- **Transcription:** `mac/receiver.py` with whisper.cpp on your Mac.

## Status

Designed, not built. The firmware compiles and the case renders in CI, but nothing has run on hardware or been printed. The checklist is in [docs/DESIGN.md](docs/DESIGN.md#validation-plan).

## Build one

1. Buy the parts and wire them: [docs/DESIGN.md](docs/DESIGN.md#parts).
2. Measure your parts, then print the case: [docs/DESIGN.md](docs/DESIGN.md#print-and-assemble).
3. Flash the firmware: [docs/DESIGN.md](docs/DESIGN.md#flash).
4. Start the receiver on your Mac: [docs/RECEIVER.md](docs/RECEIVER.md).

## Layout

| Path | What |
| --- | --- |
| `firmware/` | PlatformIO firmware for the pen |
| `cad/` | `pen.scad`, ready-made STLs, preview renders |
| `mac/receiver.py` | Mac receiver and transcriber |
| `docs/DESIGN.md` | Brief, design decisions, parts, wiring, assembly, validation plan |
| `docs/RECEIVER.md` | Receiver setup and the wire protocol |

This is the `voice-pen` branch of [coobytron/M5-Stick-Plus-1.1---Voice](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice): a separate history with nothing from the M5StickC firmware, so it can move to a repository of its own with one push. Work on the pen branches from `voice-pen` and merges back into it, not into `main`.
