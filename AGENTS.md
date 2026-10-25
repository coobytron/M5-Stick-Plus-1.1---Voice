# Agent rules for this repository

This project follows the rules in [coobytron/Agent-Cody-Banks](https://github.com/coobytron/Agent-Cody-Banks) (`AGENTS.md` there): one accountable agent, checklists instead of personas, evidence over assumptions. The plugin is enabled in `.claude/settings.json`.

Keep plans, specs and decisions as Markdown in this repository so the next session can pick up without asking again. `docs/DESIGN.md` holds the brief (goal, constraints, acceptance criteria), the design and why, parts, wiring and the validation plan. Update it in the same PR as any change to the firmware, the case or the receiver.

This tree is the `voice-pen` branch of coobytron/M5-Stick-Plus-1.1---Voice, with its own history. Branch from `voice-pen` and open PRs against `voice-pen`, never `main`.

## Project facts

- Hardware: Seeed XIAO ESP32-S3 Sense (PDM mic on GPIO 42/41, microSD CS on GPIO 21) and a Good Display GDEM0097T61 0.97" e-ink panel (184 x 88, SSD1680) on its DESPI-C02-CV0097 adapter. Pins are in `firmware/src/config.h`.
- Firmware: PlatformIO in `firmware/`. `pio run` builds `recorder` and `bench213` (a 2.13" panel for bench testing). Secrets live in `firmware/src/secrets.h`, which is git-ignored. Never commit Wi-Fi passwords.
- Visual target: `docs/reference/concept-mvp.png`, supplied by Colson as the MVP. Keep it unchanged. Where the build can't match it, record the difference and the reason in the "concept" table in `docs/DESIGN.md`; don't quietly drift from it.
- Case: `cad/recorder.scad`. After changing it, re-export `cad/stl/` (command in `docs/DESIGN.md`), re-render `cad/renders/` if the look changed, and re-run the fit check described there. Values marked `MEASURE` are not from parts in hand.
- Screens: after changing `firmware/src/screen.cpp`, run `tools/screen-preview/build.sh`, look at `out-recorder/sheet.png`, and refresh `docs/reference/screens-097.png`. The panel is only 88 px wide.
- Receiver: `mac/receiver.py`, standard library only. The same file lives in [coobytron/M5-Stick-Plus-1.1---Voice](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice); keep the wire protocol in `docs/RECEIVER.md` compatible with both devices.
- Nothing has been verified on hardware or printed until someone says so in a PR or in the validation plan of `docs/DESIGN.md`. Do not claim hardware behavior from code alone, or how a part looks from code alone: render it and look.
