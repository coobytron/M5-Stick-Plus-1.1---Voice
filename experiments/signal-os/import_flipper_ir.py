#!/usr/bin/env python3
"""Load supported Flipper Zero .ir remote signals into SIGNAL OS via USB serial.

Only send IR controls from remotes/devices you own or are authorized to test.
Requires: pip install pyserial
Example:
  python3 import_flipper_ir.py --port /dev/cu.usbserial-XXXX ~/Downloads/TV.ir
  python3 import_flipper_ir.py --dry-run ~/Downloads/TV.ir
"""
import argparse
from pathlib import Path
import re
import sys
import time

MAX_PULSES = 192
MAX_DURATION_US = 15000

def parse_signals(text):
    current = {}
    for line in text.splitlines() + ["#"]:
        line = line.strip()
        if line == "#":
            if "name" in current:
                yield current
            current = {}
        elif ":" in line and not line.startswith("Filetype:"):
            key, value = line.split(":", 1)
            current[key.strip().lower()] = value.strip()

def nec_pulses(address, command, extended=False):
    # NEC: 9ms lead mark, 4.5ms space, then 32 LSB-first bits and final mark.
    lo = address & 255
    hi = (address >> 8) & 255 if extended else (~lo) & 255
    cmd = command & 255
    data = [lo, hi, cmd, (~cmd) & 255]
    durations = [9000, 4500]
    for byte in data:
        for bit in range(8):
            durations.extend((560, 1690 if (byte >> bit) & 1 else 560))
    durations.append(560)
    return durations

def flipper_u32(s):
    # Flipper prints bytes in display order (low byte first for these protocols).
    values = [int(v, 16) for v in s.split()]
    if not 1 <= len(values) <= 4 or any(v < 0 or v > 255 for v in values):
        raise ValueError("bad hex byte list")
    return sum(v << (8 * i) for i, v in enumerate(values))

def encode_signal(signal):
    name = re.sub(r"[^ -~]", "_", signal["name"]).replace("|", " ").strip()[:21]
    if not name:
        raise ValueError("no label")
    kind = signal.get("type", "").lower()
    if kind == "raw":
        frequency = int(signal.get("frequency", "38000"))
        pulses = [int(p) for p in signal.get("data", "").split()]
    elif kind == "parsed":
        protocol = signal.get("protocol", "").upper()
        if protocol not in ("NEC", "NECEXT"):
            raise ValueError(f"unsupported parsed IR protocol {protocol}; capture it as RAW")
        address = flipper_u32(signal["address"])
        command = flipper_u32(signal["command"])
        frequency = 38000
        pulses = nec_pulses(address, command, extended=(protocol == "NECEXT"))
    else:
        raise ValueError(f"unsupported type {kind!r}")
    if not (30000 <= frequency <= 60000):
        raise ValueError("carrier outside 30-60 kHz")
    if not (2 <= len(pulses) <= MAX_PULSES):
        raise ValueError(f"pulse count {len(pulses)} outside 2-{MAX_PULSES}")
    if any(p < 65 or p > MAX_DURATION_US for p in pulses):
        raise ValueError("pulse timing outside 65-15000 us")
    return f"IRRAW|{name}|{frequency}|{','.join(map(str, pulses))}"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", type=Path, help="Flipper Infrared .ir files")
    parser.add_argument("--port", help="M5StickC Plus serial port")
    parser.add_argument("--dry-run", action="store_true", help="validate without connecting")
    args = parser.parse_args()
    if not args.dry_run and not args.port:
        parser.error("--port is required unless --dry-run is set")
    ser = None
    if not args.dry_run:
        try:
            import serial
        except ImportError:
            parser.error("Install pyserial: python3 -m pip install pyserial")
        ser = serial.Serial(args.port, 921600, timeout=4)
        time.sleep(1)
        ser.reset_input_buffer()
    success, skipped = 0, 0
    try:
        for path in args.files:
            for signal in parse_signals(path.read_text(encoding="utf-8")):
                try:
                    command = encode_signal(signal)
                    if ser:
                        ser.write((command + "\n").encode("ascii"))
                        ser.flush()
                        response = ser.readline().decode("utf-8", errors="replace").strip()
                        if response != "REMOTE OK":
                            raise RuntimeError(f"device response: {response or 'timeout'}")
                    print(f"{path.name}: {signal['name']}: {'validated' if args.dry_run else 'imported'}")
                    success += 1
                except (ValueError, RuntimeError, KeyError) as e:
                    print(f"{path.name}: {signal.get('name','?')}: SKIP: {e}", file=sys.stderr)
                    skipped += 1
    finally:
        if ser:
            ser.close()
    print(f"Total: {success} accepted, {skipped} skipped")
    return 0 if skipped == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
