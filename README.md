# M5StickC Plus 1.1 Voice

Hold a button, say something, and read it back translated. Firmware for the M5StickC Plus 1.1: it records from the built-in mic, sends the clip to OpenAI Whisper for a transcript, has Claude translate it, and shows the result on the screen.

How it works, and why, is in [docs/DESIGN.md](docs/DESIGN.md). Goals and open questions are in [docs/PROJECT-BRIEF.md](docs/PROJECT-BRIEF.md).

This repository also holds a second, free firmware: a **dictation recorder** that streams what you say to your Mac, which transcribes it locally with Whisper. It lives in `firmware/` and `mac/`; see [docs/DICTATION.md](docs/DICTATION.md).

The **voice pen**, a click-to-record pen with an e-ink window and a 3D-printed body that uses the same Mac receiver, now has its own repository: [coobytron/voice-pen](https://github.com/coobytron/voice-pen).

## Use it

| Button | Does |
|---|---|
| Hold **A** (front) | Record. Let go to translate. Up to 20 s. |
| Click **B** (side) | Next target language. On a result, re-translates it. |
| Click **power** | Scroll a long result. |

Languages: Spanish (default), English, French, German, Italian, Portuguese, Japanese, Simplified Chinese. Edit `LANGUAGES` in `src/config.h` to change the list.

## Set up

1. Install [PlatformIO](https://platformio.org/install) (the VS Code extension or `pip install platformio`).
2. Copy `include/secrets.example.h` to `include/secrets.h` and fill in:
   - your WiFi name and password (2.4 GHz only),
   - an OpenAI API key for speech-to-text,
   - an Anthropic API key for translation.
3. Plug in the stick over USB-C and run:

   ```bash
   pio run -t upload
   pio device monitor   # optional: prints each transcript and translation
   ```

`include/secrets.h` is git-ignored. The keys are stored in the stick's flash, so rotate them if you lose it.

## Cost

Each phrase is one Whisper call (billed per minute of audio) and one short Claude call. Check current prices on each provider's pricing page.

## Status

The firmware compiles in CI. It has not been run on a real stick yet; see the validation checklist in [docs/DESIGN.md](docs/DESIGN.md).
