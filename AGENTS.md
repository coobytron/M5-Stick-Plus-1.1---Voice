# Agent rules for this repository

This project follows the rules in [coobytron/Agent-Cody-Banks](https://github.com/coobytron/Agent-Cody-Banks) (`AGENTS.md` there): one accountable agent, checklists instead of personas, evidence over assumptions. The plugin is enabled in `.claude/settings.json`.

Keep plans, specs and decisions as Markdown in this repository so the next session can pick up without asking again:

- `docs/PROJECT-BRIEF.md`: goal, constraints, acceptance criteria. Update it when the goal changes.
- `docs/DESIGN.md`: how the firmware works and why. Update it in the same PR as any change to the pipeline, controls, or limits.

## Project facts

The repository holds two separate firmwares for the M5StickC Plus, plus a separate pen device:

- **Translator** (root `platformio.ini`, `src/`, `include/`): records a phrase, transcribes and translates it with cloud APIs. Docs: `docs/DESIGN.md`, `docs/PROJECT-BRIEF.md`.
- **Dictation recorder** (`firmware/`, `mac/`): streams audio over Wi-Fi to a Mac receiver that transcribes locally with whisper.cpp, free. Plan and setup: `docs/DICTATION.md`; its secrets live in `firmware/src/secrets.h` (git-ignored). Update `docs/DICTATION.md` in the same PR as any change to it.
- **Voice pen** (`pen/firmware/`, `pen/cad/`): XIAO ESP32-S3 Sense + 0.97" e-ink in a 3D-printed pen body. Records to microSD, uploads to the same Mac receiver, shows the transcript on e-ink. Brief, design, wiring and validation: `docs/PEN.md`; secrets in `pen/firmware/src/secrets.h` (git-ignored). The case is `pen/cad/pen.scad`; re-export `pen/cad/stl/` when it changes. Update `docs/PEN.md` in the same PR as any change to it.

Translator facts:

- Target: M5StickC Plus 1.1 (ESP32-PICO-D4, no PSRAM, PDM mic on G0/G34, ST7789 135 x 240).
- Build: PlatformIO, `pio run`. Flash: `pio run -t upload`. CI builds every push.
- Secrets live in `include/secrets.h`, which is git-ignored. Never commit keys.
- Nothing has been verified on hardware until someone says so in a PR or in `docs/DESIGN.md` "Validation plan". Do not claim hardware behavior from code alone.
