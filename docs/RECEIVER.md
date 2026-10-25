# Mac receiver

`mac/receiver.py` takes recordings from the voice recorder, transcribes them on your Mac with [whisper.cpp](https://github.com/ggerganov/whisper.cpp), saves each transcript as a text file and sends the text back so the recorder can title the note in Files. Nothing leaves your Mac and it costs nothing.

The same receiver also serves the [M5StickC dictation recorder](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice). Both repositories carry a copy; keep them in step when one changes.

## Set up

You need a Mac with [Homebrew](https://brew.sh), on the same Wi-Fi as the recorder.

```sh
brew install whisper-cpp
python3 mac/receiver.py
```

The first run downloads the Whisper speech model (about 470 MB), and macOS asks whether Python may accept incoming connections. Click **Allow**. Transcripts are saved in `~/Dictation`, named by the date and time you spoke, and a notification pops up for each one.

| Option | What it does |
| --- | --- |
| `--out FOLDER` | Where transcripts go (default `~/Dictation`) |
| `--model NAME` | `base.en` (fastest), `small.en` (default), `medium.en`, or `large-v3-turbo` (best; multilingual; use on Apple Silicon) |
| `--language CODE` | Spoken language for multilingual models, e.g. `es`. Default is auto-detect |
| `--keep-audio` | Keep the .wav recordings in `~/.m5dictation/recordings/done` |

If a transcription fails or the receiver is closed before it finishes, the audio stays in `~/.m5dictation/recordings` and is transcribed the next time the receiver starts.

## Troubleshooting

- **Recorder says "Mac not found"**: some routers block Bonjour. Set `DICTATION_HOST` in `firmware/src/secrets.h` to your Mac's IP (System Settings > Wi-Fi > Details).
- **Transcripts miss words**: run with `--keep-audio` and listen to the .wav. If it's too quiet or distorted, change `kMicGain` in `firmware/src/config.h`. If the audio is fine, try a bigger `--model`.

## Protocol

Plain TCP on port 5730, advertised over Bonjour as `_m5dict._tcp`. The device sends one header line, `M5DICT/1 rate=16000 bits=16 ch=1\n`, then frames of `[uint16 LE length][PCM bytes]` (16-bit mono), then a zero-length frame. The receiver answers `OK\n`.

Two optional header fields, used by the recorder because it records offline and uploads later:

- `ts=<unix seconds>`: when the recording started. Audio and transcript are named by this time instead of the arrival time.
- `reply=text`: after `OK\n`, the receiver keeps the connection open until the recording is transcribed (up to 85 s) and sends `TEXT <transcript>\n`: one line of plain ASCII, at most 280 characters, empty if there was no speech or it took too long.

Receivers that don't know these fields ignore them.
