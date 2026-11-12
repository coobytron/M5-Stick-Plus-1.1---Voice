# M5StickC Plus 1.1 Voice

Hold a button, say something, and read it back translated. Firmware for the M5StickC Plus 1.1: it records from the built-in mic, sends the clip to OpenAI Whisper for a transcript, has Claude translate it, and shows the result on the screen.

How it works, and why, is in [docs/DESIGN.md](docs/DESIGN.md). Goals and open questions are in [docs/PROJECT-BRIEF.md](docs/PROJECT-BRIEF.md).

This repository also holds a second, free firmware: a **dictation recorder** that streams what you say to your Mac, which transcribes it locally with Whisper. It lives in `firmware/` and `mac/`; see [docs/DICTATION.md](docs/DICTATION.md).

The **pocket voice recorder** (one button, an e-ink window and a 3D-printed case; it started here as the voice pen) uses the same Mac receiver and now has its own repository: [coobytron/voice-pen](https://github.com/coobytron/voice-pen).

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

## SIGNAL OS (separate instrument firmware)

The [`experiments/signal-os/`](experiments/signal-os/) folder contains **SIGNAL OS v3.1 + Trail Echo**: BLE Radar, Wi-Fi Radar, Invisible Weather, Audio Scope, USB Voice Notes, and the IMU-based Trail Echo terrain visualization. See [Signal OS status and validation](docs/SIGNAL-OS.md). This is an independent experimental sketch; existing translator and dictation firmware remain unchanged. Host tests pass; full Arduino/device validation remains outstanding. This source is based on the supplied v3.1 sketch and does not include v4 Channel Atlas.

## SIGNAL OS RF424 + MTB instrumentation (new prototype)

See [the SIGNAL OS instruments and setup](experiments/signal-os/README.md) and [architecture and validation](docs/SIGNAL-OS.md). The original **M5StickC Plus 1.x / PCB revision 1.2** is required—**not Plus2**. This prototype adds **RF Atlas (receive-only CC1101 433 MHz)**, **Channel Atlas (Wi-Fi AP channel strengths + optional RF)**, RF-energy effects in **Invisible Weather**, a strictly non-identifying **RF Quiet Watch** alongside the existing BLE Lost Alert, and a visible animated **Trail Echo** recording screen. The new CC1101 integration is **disabled by default** (`SIGNAL_OS_RF424_CC1101=0`) to preserve the already-working standalone stick. For actual 433 MHz measurements, install RadioLib, enable the flag, select the hardware's 433 MHz switch position, and verify pins on the original Plus 1.x. RadioLib/device compile and hardware validation are still pending. The RF424's nRF24 and the Cardputer ADV SX1262 LoRa cannot directly interoperate; the GPS Bridge uses ESP-NOW separately. No transmit, jamming, source identification, or guaranteed safety alert is provided.

**RadioLib compatibility:** install exactly **RadioLib 7.8.1** for the opt-in CC1101 receiver. RadioLib **8.0.0** (released November 7, 2026) changed `CC1101::begin` and frequency API types; the header intentionally rejects 8.x until its adapter is ported. With the RF flag left at `0`, RadioLib is not required.
