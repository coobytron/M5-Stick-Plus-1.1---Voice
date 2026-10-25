#!/usr/bin/env python3
"""Dictation receiver for the M5StickC Plus, for macOS.

Receives recordings streamed from the Stick, transcribes them locally with
whisper.cpp, and saves each transcript as a text file. Nothing leaves your Mac.

    brew install whisper-cpp
    python3 mac/receiver.py                       # transcripts in ~/Dictation
    python3 mac/receiver.py --out ~/Desktop/notes
    python3 mac/receiver.py --model large-v3-turbo   # better, needs Apple Silicon

Only the Python standard library is used. The Whisper model is downloaded the
first time it's needed.
"""

import argparse
import datetime
import pathlib
import queue
import shutil
import socketserver
import struct
import subprocess
import sys
import threading
import unicodedata
import urllib.request
import wave

PORT = 5730
HOME = pathlib.Path.home() / ".m5dictation"
MODEL_URL = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-{}.bin"
MIN_SECONDS = 0.5  # ignore accidental double-taps
REPLY_WAIT_SECONDS = 85  # the voice pen gives up after 90 s
REPLY_MAX_CHARS = 280    # the pen's e-ink window shows the first few lines


class Job:
    """One recording to transcribe. If someone is waiting on the text (the
    voice pen's reply=text), done is set once text holds the transcript."""

    def __init__(self, path, seconds, wants_reply=False):
        self.path = path
        self.seconds = seconds
        self.text = ""
        self.done = threading.Event() if wants_reply else None

    def finish(self, text=""):
        self.text = text
        if self.done:
            self.done.set()


jobs: "queue.Queue[Job]" = queue.Queue()


# ---------------------------------------------------------------------------
# Receiving audio from the Stick


def read_exact(rfile, n):
    data = rfile.read(n)
    if len(data) != n:
        raise ConnectionError("Stick disconnected mid-recording")
    return data


class StickHandler(socketserver.StreamRequestHandler):
    audio_dir: pathlib.Path

    def handle(self):
        header = self.rfile.readline().decode("ascii", "replace").strip()
        if not header.startswith("M5DICT/1"):
            log(f"ignoring unknown client: {header!r}")
            return
        fields = dict(part.split("=", 1) for part in header.split()[1:] if "=" in part)
        rate = int(fields.get("rate", 16000))
        wants_reply = fields.get("reply") == "text"
        # The voice pen records offline and uploads later; ts says when it was said.
        ts = int(fields.get("ts", 0))
        when = datetime.datetime.fromtimestamp(ts) if ts > 0 else datetime.datetime.now()

        path = unique_audio_path(self.audio_dir, when)
        log(f"recording from {self.client_address[0]}")

        total = 0
        complete = False
        try:
            with wave.open(str(path), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(rate)
                while True:
                    (length,) = struct.unpack("<H", read_exact(self.rfile, 2))
                    if length == 0:
                        break
                    wav.writeframes(read_exact(self.rfile, length))
                    total += length
        except ConnectionError as e:
            # Keep whatever arrived: a dropped connection shouldn't lose the idea.
            log(f"{e}; transcribing what arrived")
        else:
            self.wfile.write(b"OK\n")
            complete = True

        seconds = total / (rate * 2)
        job = Job(path, seconds, wants_reply and complete)
        if seconds < MIN_SECONDS:
            path.unlink(missing_ok=True)
            log("recording too short, skipped")
            job.finish()
        else:
            log(f"got {seconds:.0f} s of audio, queued for transcription")
            jobs.put(job)

        if job.done:
            job.done.wait(REPLY_WAIT_SECONDS)
            try:
                self.wfile.write(f"TEXT {for_small_screen(job.text)}\n".encode("ascii"))
            except OSError:
                pass  # the pen stopped waiting; the transcript is saved anyway


def unique_audio_path(audio_dir, when):
    """dictation_<time>.wav, with _2, _3... when several arrive for the same second."""
    stem = f"dictation_{when:%Y-%m-%d_%H-%M-%S}"
    path = audio_dir / f"{stem}.wav"
    n = 2
    while path.exists():
        path = audio_dir / f"{stem}_{n}.wav"
        n += 1
    return path


def for_small_screen(text):
    """Plain ASCII on one line: the pen's e-ink fonts have no accents or curly quotes."""
    for fancy, plain in {"\u2018": "'", "\u2019": "'", "\u201c": '"', "\u201d": '"',
                         "\u2013": "-", "\u2014": "-", "\u2026": "..."}.items():
        text = text.replace(fancy, plain)
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode()
    text = " ".join(text.split())
    return text[:REPLY_MAX_CHARS]


# ---------------------------------------------------------------------------
# Transcription


def find_whisper():
    for name in ("whisper-cli", "whisper-cpp"):
        exe = shutil.which(name)
        if exe:
            return exe
    sys.exit("whisper.cpp not found. Install it with:  brew install whisper-cpp")


def ensure_model(name):
    path = HOME / "models" / f"ggml-{name}.bin"
    if path.exists():
        return path
    path.parent.mkdir(parents=True, exist_ok=True)
    url = MODEL_URL.format(name)
    log(f"downloading Whisper model '{name}' (one time only)...")
    partial = path.with_suffix(".part")
    try:
        with urllib.request.urlopen(url) as resp, open(partial, "wb") as f:
            size = int(resp.headers.get("Content-Length", 0))
            done = 0
            while chunk := resp.read(1 << 20):
                f.write(chunk)
                done += len(chunk)
                if size:
                    print(f"\r  {done * 100 // size}% of {size >> 20} MB", end="", flush=True)
        print()
    except Exception as e:
        partial.unlink(missing_ok=True)
        sys.exit(f"couldn't download {url}: {e}")
    partial.rename(path)
    return path


def transcribe(whisper, model, language, wav_path):
    """Runs whisper.cpp on a .wav and returns the transcript text."""
    out_base = wav_path.with_suffix("")
    result = subprocess.run(
        [whisper, "-m", str(model), "-f", str(wav_path), "-l", language,
         "-nt", "-np", "-otxt", "-of", str(out_base)],
        capture_output=True, text=True,
    )
    txt = out_base.with_suffix(".txt")
    if result.returncode != 0 or not txt.exists():
        raise RuntimeError(result.stderr.strip()[-500:] or "whisper.cpp failed")
    text = txt.read_text().strip()
    txt.unlink()
    # whisper.cpp writes one line per segment; join them into paragraphs-ish prose.
    return " ".join(line.strip() for line in text.splitlines() if line.strip())


def save_transcript(out_dir, wav_path, seconds, text):
    when = datetime.datetime.strptime(wav_path.stem[:29], "dictation_%Y-%m-%d_%H-%M-%S")
    path = out_dir / f"{when:%Y-%m-%d %H.%M.%S} dictation.txt"
    n = 2
    while path.exists():  # several recordings from the same second
        path = out_dir / f"{when:%Y-%m-%d %H.%M.%S} dictation {n}.txt"
        n += 1
    mins, secs = divmod(round(seconds), 60)
    path.write_text(f"Dictation, {when:%A %d %B %Y at %H:%M} ({mins}:{secs:02d})\n\n{text}\n")
    return path


def notify(title, message):
    """Shows a macOS notification. Silently does nothing on other systems."""
    if not shutil.which("osascript"):
        return
    message = message.replace("\\", "\\\\").replace('"', '\\"')
    subprocess.run(
        ["osascript", "-e", f'display notification "{message}" with title "{title}"'],
        capture_output=True,
    )


def transcribe_worker(whisper, model, language, out_dir, keep_audio):
    while True:
        job = jobs.get()
        wav_path, seconds = job.path, job.seconds
        text = ""
        try:
            log(f"transcribing {wav_path.name}...")
            text = transcribe(whisper, model, language, wav_path)
            if text:
                path = save_transcript(out_dir, wav_path, seconds, text)
                log(f"saved {path}")
                print(f"\n{text}\n", flush=True)
                notify("Dictation ready", text[:120])
            else:
                log("no speech found")
                notify("Dictation", "No speech found in that recording.")
            if keep_audio:
                done = wav_path.parent / "done"
                done.mkdir(exist_ok=True)
                wav_path.rename(done / wav_path.name)
            else:
                wav_path.unlink(missing_ok=True)
        except Exception as e:
            log(f"transcription failed, audio kept at {wav_path}: {e}")
            notify("Dictation failed", str(e)[:120])
        finally:
            job.finish(text)
            jobs.task_done()


# ---------------------------------------------------------------------------


def log(msg):
    print(f"[{datetime.datetime.now():%H:%M:%S}] {msg}", flush=True)


def advertise():
    """Registers a Bonjour service with macOS's built-in dns-sd tool."""
    if not shutil.which("dns-sd"):
        log("dns-sd not found: set DICTATION_HOST in secrets.h to this machine's IP")
        return None
    return subprocess.Popen(
        ["dns-sd", "-R", "M5 Dictation", "_m5dict._tcp", "local", str(PORT)],
        stdout=subprocess.DEVNULL,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", default="~/Dictation", help="folder for transcripts (default ~/Dictation)")
    parser.add_argument("--model", default="small.en",
                        help="Whisper model: base.en, small.en (default), medium.en, large-v3-turbo")
    parser.add_argument("--language", default=None,
                        help="spoken language code, e.g. en, es (default: en for .en models, else auto)")
    parser.add_argument("--keep-audio", action="store_true",
                        help="keep .wav files after a successful transcript")
    args = parser.parse_args()

    out_dir = pathlib.Path(args.out).expanduser()
    out_dir.mkdir(parents=True, exist_ok=True)
    StickHandler.audio_dir = HOME / "recordings"
    StickHandler.audio_dir.mkdir(parents=True, exist_ok=True)
    language = args.language or ("en" if args.model.endswith(".en") else "auto")

    whisper = find_whisper()
    model = ensure_model(args.model)

    # Recordings that never got transcribed (receiver quit, whisper failed) are retried.
    for leftover in sorted(StickHandler.audio_dir.glob("dictation_*.wav")):
        with wave.open(str(leftover)) as w:
            jobs.put(Job(leftover, w.getnframes() / w.getframerate()))

    threading.Thread(
        target=transcribe_worker,
        args=(whisper, model, language, out_dir, args.keep_audio),
        daemon=True,
    ).start()

    socketserver.ThreadingTCPServer.allow_reuse_address = True
    socketserver.ThreadingTCPServer.daemon_threads = True
    try:
        server = socketserver.ThreadingTCPServer(("", PORT), StickHandler)
    except OSError as e:
        sys.exit(f"can't listen on port {PORT} ({e.strerror}). Is the receiver already running?")
    bonjour = advertise()
    with server:
        log(f"ready: model {args.model}, transcripts go to {out_dir}")
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            if bonjour:
                bonjour.terminate()


if __name__ == "__main__":
    main()
