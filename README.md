# M5StickC Plus dictation

Press record on an M5StickC Plus 1.1, dictate ideas, and get a full transcript on your Mac and iPhone. Everything runs on hardware you own, for free.

```
M5StickC Plus  --Wi-Fi audio stream-->  Mac (Whisper)  -->  Apple Notes / iCloud  -->  iPhone
```

## Roadmap

1. **Recording firmware** (done): button A starts and stops, audio streams to the Mac as you talk.
2. **Mac receiver with Whisper** (done): each recording is transcribed locally and saved as a text file.
3. **Delivery**: save transcripts to Apple Notes or iCloud Drive so they sync to iPhone.
4. **Polish**: on-device Wi-Fi setup, battery handling, an M5Burner-flashable .bin.

## Setup

You need a Mac with [Homebrew](https://brew.sh), and the Stick on the same Wi-Fi.

### 1. Install the tools (once)

```sh
brew install platformio whisper-cpp
```

### 2. Flash the Stick

1. Copy `firmware/src/secrets.example.h` to `firmware/src/secrets.h` and put in your Wi-Fi name and password.
2. Plug in the Stick and run:
   ```sh
   cd firmware
   pio run -t upload
   ```

### 3. Start the receiver on your Mac

```sh
python3 mac/receiver.py
```

The first run downloads the Whisper speech model (about 470 MB), and macOS asks whether Python may accept incoming connections. Click **Allow**.

### 4. Dictate

The Stick shows **Ready** once it finds the Mac. Press **A** (the big front button), talk, and press **A** again. A few seconds later a notification pops up on the Mac, and the transcript is saved in `~/Dictation` as a text file named by date and time.

### Receiver options

| Option | What it does |
| --- | --- |
| `--out FOLDER` | Where transcripts go (default `~/Dictation`) |
| `--model NAME` | `base.en` (fastest), `small.en` (default), `medium.en`, or `large-v3-turbo` (best; multilingual; use on Apple Silicon) |
| `--language CODE` | Spoken language for multilingual models, e.g. `es`. Default is auto-detect |
| `--keep-audio` | Keep the .wav recordings in `~/.m5dictation/recordings/done` |

If a transcription fails or the receiver is closed before it finishes, the audio stays in `~/.m5dictation/recordings` and is transcribed the next time the receiver starts.

### On the Stick

| Screen | Meaning |
| --- | --- |
| Wi-Fi... | Joining your network |
| Find Mac | Looking for the receiver over Bonjour. Is it running? |
| Ready | Press A to record |
| REC 00:12 (red) | Recording; the bar shows mic level, the red LED is on |
| Sent! | The Mac has the audio |

### Troubleshooting

- **Stuck on "Find Mac"**: some routers block Bonjour. Set `DICTATION_HOST` in `secrets.h` to your Mac's IP (System Settings > Wi-Fi > Details).
- **Transcripts miss words**: try `--keep-audio` and listen to the .wav. If it's too quiet or distorted, change `kMicGain` in `firmware/src/main.cpp` (default 8). If the audio is fine, try a bigger `--model`.
- **"weak Wi-Fi" while recording**: the Stick's buffer is filling faster than Wi-Fi can send, so some audio may be lost. Move closer to the router.

## How audio is sent

Plain TCP on port 5730. The Stick sends `M5DICT/1 rate=16000 bits=16 ch=1\n`, then frames of `[uint16 LE length][PCM bytes]` (16-bit mono, 16 kHz), then a zero-length frame. The receiver answers `OK\n`.
