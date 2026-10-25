#!/usr/bin/env python3
"""Minimal Mac-side receiver for testing the Stick firmware.

Saves each recording as a .wav file so you can listen back and check the mic
works. The real receiver (step 2) adds Whisper transcription on top of this.

    python3 tools/test_receiver.py              # saves into ./recordings
    python3 tools/test_receiver.py --out ~/Desktop

On macOS it advertises itself over Bonjour so the Stick finds it automatically.
"""

import argparse
import datetime
import pathlib
import shutil
import socketserver
import struct
import subprocess
import wave

PORT = 5730


def read_exact(rfile, n):
    data = rfile.read(n)
    if len(data) != n:
        raise ConnectionError("stick disconnected mid-recording")
    return data


class DictationHandler(socketserver.StreamRequestHandler):
    out_dir: pathlib.Path

    def handle(self):
        header = self.rfile.readline().decode("ascii", "replace").strip()
        if not header.startswith("M5DICT/1"):
            print(f"ignoring unknown client: {header!r}")
            return
        fields = dict(part.split("=", 1) for part in header.split()[1:])
        rate = int(fields.get("rate", 16000))

        stamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        path = self.out_dir / f"dictation_{stamp}.wav"
        print(f"recording from {self.client_address[0]} -> {path.name}")

        with wave.open(str(path), "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(rate)
            total = 0
            while True:
                (length,) = struct.unpack("<H", read_exact(self.rfile, 2))
                if length == 0:
                    break
                wav.writeframes(read_exact(self.rfile, length))
                total += length

        self.wfile.write(b"OK\n")
        print(f"saved {total / (rate * 2):.1f} s")


def advertise():
    """Register a Bonjour service with macOS's built-in dns-sd tool."""
    if not shutil.which("dns-sd"):
        print("dns-sd not found: set DICTATION_HOST in secrets.h to this machine's IP")
        return None
    return subprocess.Popen(
        ["dns-sd", "-R", "M5 Dictation", "_m5dict._tcp", "local", str(PORT)],
        stdout=subprocess.DEVNULL,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", default="recordings", help="folder for .wav files")
    args = parser.parse_args()

    DictationHandler.out_dir = pathlib.Path(args.out).expanduser()
    DictationHandler.out_dir.mkdir(parents=True, exist_ok=True)

    bonjour = advertise()
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer(("", PORT), DictationHandler) as server:
        print(f"listening on port {PORT}, saving to {DictationHandler.out_dir}")
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            if bonjour:
                bonjour.terminate()


if __name__ == "__main__":
    main()
