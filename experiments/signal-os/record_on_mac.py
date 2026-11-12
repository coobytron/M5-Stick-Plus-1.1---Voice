#!/usr/bin/env python3
"""Receive M5Stick SIGNAL OS voice notes over USB serial and save WAV files.

Usage:
    python3 record_on_mac.py /dev/cu.SLAB_USBtoUART
Requires: pyserial (`python3 -m pip install pyserial` in a venv).

Start this program, enter VOICE REC on your M5Stick, press B to record,
press B again to stop. Files go to ./recordings/ by default.
"""
import argparse
import datetime as dt
import pathlib
import struct
import sys
import time
import wave

try:
    import serial
except ImportError:
    print("Missing pyserial. In a virtual environment: pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


def read_exact(port, length):
    buf = bytearray()
    deadline = time.monotonic() + 4
    while len(buf) < length:
        fragment = port.read(length - len(buf))
        if fragment:
            buf.extend(fragment)
            deadline = time.monotonic() + 4
        elif time.monotonic() > deadline:
            raise TimeoutError("USB serial audio stream stopped unexpectedly")
    return bytes(buf)


def main():
    p = argparse.ArgumentParser(description="SIGNAL OS M5Stick USB audio receiver")
    p.add_argument("port", help="Serial port from Arduino Tools > Port")
    p.add_argument("--out", default="recordings", help="Directory to save WAV files")
    a = p.parse_args()
    destination = pathlib.Path(a.out)
    destination.mkdir(parents=True, exist_ok=True)
    print(f"Connecting to {a.port} at 921600 baud. Close Arduino Serial Monitor first.")
    with serial.Serial(a.port, baudrate=921600, timeout=0.1, write_timeout=2) as ser:
        # Opening the serial port may reset the M5Stick; allow it to boot.
        time.sleep(1.5)
        ser.reset_input_buffer()
        seek = bytearray()
        last_hello = 0.0
        while True:
            if time.monotonic() - last_hello >= 1:
                ser.write(b"HELLO\n")
                last_hello = time.monotonic()
            b = ser.read(1)
            if not b:
                continue
            seek.extend(b)
            if len(seek) > 4:
                del seek[:-4]
            if seek != b"SREC":
                continue
            sample_rate = struct.unpack("<I", read_exact(ser, 4))[0]
            if sample_rate != 16000:
                print(f"Unexpected sampling rate {sample_rate}; ignoring stream")
                seek.clear()
                continue
            stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
            path = destination / f"signal_{stamp}.wav"
            frame_bytes = 0
            print(f"RECORDING → {path} (press B again on M5Stick to stop)")
            try:
                with wave.open(str(path), "wb") as wav:
                    wav.setnchannels(1)
                    wav.setsampwidth(2)
                    wav.setframerate(sample_rate)
                    while True:
                        frame_type = read_exact(ser, 1)
                        if frame_type == b"D":
                            data_size = struct.unpack("<H", read_exact(ser, 2))[0]
                            if not 0 < data_size <= 4096 or data_size % 2:
                                raise ValueError(f"Invalid audio packet length: {data_size}")
                            data = read_exact(ser, data_size)
                            wav.writeframesraw(data)
                            frame_bytes += data_size
                        elif frame_type == b"E":
                            expected = struct.unpack("<I", read_exact(ser, 4))[0]
                            if expected != frame_bytes:
                                print(f"Warning: received {frame_bytes} bytes, device reports {expected}")
                            print(f"SAVED {path} ({frame_bytes/(sample_rate*2):.1f} seconds)")
                            break
                        else:
                            raise ValueError(f"Unexpected audio packet: {frame_type!r}")
            except (TimeoutError, ValueError) as exc:
                print(f"Recording interrupted: {exc}; partial file saved to {path}", file=sys.stderr)
            seek.clear()
            last_hello = 0


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nAudio receiver stopped.")
