# Project Brief

Based on the Agent Cody Banks template (`templates/PROJECT-BRIEF.md` in [coobytron/Agent-Cody-Banks](https://github.com/coobytron/Agent-Cody-Banks)).

## Project

- Name: M5StickC Plus 1.1 Voice
- Owner: Colson (`coobytron`)
- Repository: [coobytron/M5-Stick-Plus-1.1---Voice](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice)
- Status: first firmware version in review, not yet tested on hardware

## Goal

A pocket voice recorder on the M5StickC Plus 1.1 that translates what was said and shows the translation on its screen.

## Audience and use context

- Primary audience: Colson, carrying the stick.
- Situation of use: short phrases spoken into the stick, read back a few seconds later, with WiFi available.
- Desired response: "it understood me and I can read it in the other language."

## Deliverables

- PlatformIO firmware (`src/`, `platformio.ini`)
- Design note (`docs/DESIGN.md`)
- Setup instructions (`README.md`)
- CI build (`.github/workflows/build.yml`)

## Constraints

- Platform and tools: ESP32-PICO-D4, Arduino framework, PlatformIO, M5Unified.
- Technical constraints: no PSRAM, no SD card, 135 x 240 screen, three buttons.
- Privacy: audio and text leave the device for OpenAI and Anthropic. API keys are stored on the device.
- Accessibility: large text for the translation; the original transcript in smaller text.

## Non-goals (first version)

- Offline speech recognition or translation.
- Audio playback.
- A companion phone app or relay server.

## Source-of-truth hierarchy

1. Colson's direction in the project thread.
2. This brief and `docs/DESIGN.md`.
3. The code and CI.

## Risks, assumptions, and unknowns

- Assumed: target language defaults to Spanish, selectable on device. Needs Colson's confirmation.
- Assumed: OpenAI Whisper for speech-to-text and Claude for translation. Needs Colson's confirmation that both API keys are available or wanted.
- Unknown until run on hardware: mic gain, end-to-end latency, glyph coverage of the fonts, TLS root coverage.

## Acceptance criteria

- [x] Firmware compiles in CI.
- [ ] Holding A records; releasing A shows a translation of what was said.
- [ ] Clicking B changes the target language and re-translates the last phrase.
- [ ] Errors (no WiFi, bad key, too short) are shown on screen in words.

## Checklists and delegation

Checklists that apply (see `checklists/INDEX.md` in Agent Cody Banks):

- `technology/ARCHITECT.md`: system boundaries, failure behavior, security and privacy (covered in `docs/DESIGN.md`).
- `technology/AI-SPECIALIST.md`: model choice and prompt.
- `principles/10-AS-LITTLE-DESIGN-AS-POSSIBLE.md`: three buttons, one screen per state.

No delegation was used.
