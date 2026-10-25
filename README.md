# M5StickC Plus dictation

Press record on an M5StickC Plus 1.1, dictate ideas, and get a full transcript on your Mac and iPhone. Everything runs on hardware you own, for free.

```
M5StickC Plus  --Wi-Fi audio stream-->  Mac (Whisper)  -->  Apple Notes / iCloud  -->  iPhone
```

## Roadmap

1. **Recording firmware** (done): button A starts and stops, audio streams to the Mac as you talk.
2. **Mac receiver with Whisper**: transcribe each recording locally.
3. **Delivery**: save transcripts to Apple Notes or iCloud Drive so they sync to iPhone.
4. **Polish**: on-device Wi-Fi setup, battery handling, an M5Burner-flashable .bin.

## Step 1: try the firmware

You need [PlatformIO](https://platformio.org/install/cli) (`brew install platformio`) and Python 3 on your Mac.

1. Copy `firmware/src/secrets.example.h` to `firmware/src/secrets.h` and put in your Wi-Fi name and password.
2. Plug in the Stick and flash it:
   ```sh
   cd firmware
   pio run -t upload
   ```
3. Start the test receiver on your Mac (same Wi-Fi as the Stick):
   ```sh
   python3 tools/test_receiver.py --out ~/Desktop/dictation
   ```
   The first time, macOS asks whether Python may accept incoming connections. Click Allow.
4. The Stick shows **Ready** once it finds the Mac. Press **A** (the big front button), talk, and press **A** again. A `.wav` file appears in the folder. Listen to it to check the mic level.

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
- **Recording too quiet or distorted**: change `kMicGain` in `firmware/src/main.cpp` (default 8).
- **"weak Wi-Fi" while recording**: the Stick's buffer is filling faster than Wi-Fi can send, so some audio may be lost. Move closer to the router.

## How audio is sent

Plain TCP on port 5730. The Stick sends `M5DICT/1 rate=16000 bits=16 ch=1\n`, then frames of `[uint16 LE length][PCM bytes]` (16-bit mono, 16 kHz), then a zero-length frame. The receiver answers `OK\n`.
