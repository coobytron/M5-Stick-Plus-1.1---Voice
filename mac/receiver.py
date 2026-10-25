#!/usr/bin/env python3
"""Dictation receiver for the M5StickC Plus, for macOS.

Receives recordings streamed from the Stick, transcribes them locally with
whisper.cpp, and saves each transcript as a text file. Nothing leaves your Mac.

    brew install whisper-cpp
    python3 mac/receiver.py                       # transcripts in ~/Dictation
    python3 mac/receiver.py --out ~/Desktop/notes
    python3 mac/receiver.py --model large-v3-turbo   # better, needs Apple Silicon
    python3 mac/receiver.py --no-notes            # text files only, skip Apple Notes

Only the Python standard library is used. The Whisper model is downloaded the
first time it's needed.
"""

import argparse
import datetime
import html
import pathlib
import queue
import shutil
import socketserver
import struct
import subprocess
import sys
import threading
import urllib.request
import wave

PORT = 5730
HOME = pathlib.Path.home() / ".m5dictation"
MODEL_URL = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-{}.bin"
MIN_SECONDS = 0.5  # ignore accidental double-taps

jobs: "queue.Queue[tuple[pathlib.Path, float]]" = queue.Queue()


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
        fields = dict(part.split("=", 1) for part in header.split()[1:])
        rate = int(fields.get("rate", 16000))

        stamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        path = self.audio_dir / f"dictation_{stamp}.wav"
        log(f"recording from {self.client_address[0]}")

        total = 0
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

        seconds = total / (rate * 2)
        if seconds < MIN_SECONDS:
            path.unlink(missing_ok=True)
            log("recording too short, skipped")
            return
        log(f"got {seconds:.0f} s of audio, queued for transcription")
        jobs.put((path, seconds))


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


def describe(wav_path, seconds):
    """Returns a line like "Sunday 25 October 2026 at 03:29 (1:05)"."""
    when = datetime.datetime.strptime(wav_path.stem, "dictation_%Y-%m-%d_%H-%M-%S")
    mins, secs = divmod(round(seconds), 60)
    return when, f"{when:%A %d %B %Y at %H:%M} ({mins}:{secs:02d})"


def save_transcript(out_dir, wav_path, seconds, text):
    when, info = describe(wav_path, seconds)
    path = out_dir / f"{when:%Y-%m-%d %H.%M.%S} dictation.txt"
    path.write_text(f"Dictation, {info}\n\n{text}\n")
    return path


# Values arrive as arguments rather than being pasted into the script, so a
# transcript can never be run as AppleScript.
NOTES_SCRIPT = """
on run argv
    set folderName to item 1 of argv
    set noteBody to item 2 of argv
    tell application "Notes"
        set acct to default account
        if not (exists folder folderName of acct) then
            make new folder at acct with properties {name:folderName}
        end if
        make new note at folder folderName of acct with properties {body:noteBody}
    end tell
end run
"""


def note_title(text, words=8):
    parts = text.split()
    title = " ".join(parts[:words])
    return title + "..." if len(parts) > words else title


def save_to_notes(folder, wav_path, seconds, text):
    """Adds the transcript to Apple Notes, which syncs it to iPhone via iCloud."""
    _, info = describe(wav_path, seconds)
    body = (f"<h1>{html.escape(note_title(text))}</h1>"
            f"<p><i>Dictated {html.escape(info)}</i></p>"
            f"<p>{html.escape(text)}</p>")
    script = [arg for line in NOTES_SCRIPT.strip().splitlines() for arg in ("-e", line.strip())]
    result = subprocess.run(["osascript", *script, folder, body], capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "osascript failed")


def notify(title, message):
    """Shows a macOS notification. Silently does nothing on other systems."""
    if not shutil.which("osascript"):
        return
    message = message.replace("\\", "\\\\").replace('"', '\\"')
    subprocess.run(
        ["osascript", "-e", f'display notification "{message}" with title "{title}"'],
        capture_output=True,
    )


def transcribe_worker(whisper, model, language, out_dir, keep_audio, notes_folder):
    while True:
        wav_path, seconds = jobs.get()
        try:
            log(f"transcribing {wav_path.name}...")
            text = transcribe(whisper, model, language, wav_path)
            if text:
                path = save_transcript(out_dir, wav_path, seconds, text)
                log(f"saved {path}")
                print(f"\n{text}\n", flush=True)
                if notes_folder:
                    try:
                        save_to_notes(notes_folder, wav_path, seconds, text)
                        log(f"added to Notes folder '{notes_folder}'")
                    except Exception as e:
                        # The text file is already saved, so don't retry the transcription.
                        log(f"couldn't add to Notes ({e}); the text file is still saved")
                        notify("Dictation saved, Notes failed", str(e)[:120])
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
    parser.add_argument("--notes-folder", default="Dictation",
                        help="Apple Notes folder for transcripts (default Dictation)")
    parser.add_argument("--no-notes", action="store_true",
                        help="don't add transcripts to Apple Notes, only save text files")
    parser.add_argument("--keep-audio", action="store_true",
                        help="keep .wav files after a successful transcript")
    args = parser.parse_args()

    out_dir = pathlib.Path(args.out).expanduser()
    out_dir.mkdir(parents=True, exist_ok=True)
    StickHandler.audio_dir = HOME / "recordings"
    StickHandler.audio_dir.mkdir(parents=True, exist_ok=True)
    language = args.language or ("en" if args.model.endswith(".en") else "auto")

    notes_folder = None
    if not args.no_notes:
        if shutil.which("osascript"):
            notes_folder = args.notes_folder
        else:
            log("osascript not found (not a Mac?): skipping Apple Notes")

    whisper = find_whisper()
    model = ensure_model(args.model)

    # Recordings that never got transcribed (receiver quit, whisper failed) are retried.
    for leftover in sorted(StickHandler.audio_dir.glob("dictation_*.wav")):
        with wave.open(str(leftover)) as w:
            jobs.put((leftover, w.getnframes() / w.getframerate()))

    threading.Thread(
        target=transcribe_worker,
        args=(whisper, model, language, out_dir, args.keep_audio, notes_folder),
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
        where = f"{out_dir} and Notes folder '{notes_folder}'" if notes_folder else str(out_dir)
        log(f"ready: model {args.model}, transcripts go to {where}")
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            if bonjour:
                bonjour.terminate()


if __name__ == "__main__":
    main()
