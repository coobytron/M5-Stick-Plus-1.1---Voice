# SIGNAL OS v3.1 (compiler fix) — M5StickC Plus 1.x

A small pocket-instrument interface with a graphite and acid-green palette.
**Fix in v3.1:** Renamed custom color constants `WHITE`, `RED`, and `ORANGE` to `UI_WHITE`, `UI_RED`, and `UI_ORANGE` to prevent ambiguity with M5GFX color constants. No intended behavior changes.

Designed for the original **M5StickC Plus / Plus 1.2 (135×240 portrait, 4 MB flash)** — not the Plus2.

## Included

- **Bluetooth**: Precision Sonar-style radar, named device list, inspection panel, RSSI history, and selected-device lost-signal watch. RSSI is approximate. Visual device angles are not real direction measurements.
- **Wi-Fi Radar**: scans nearby 2.4 GHz network access points without joining them; dots based on RSSI, with a channel/strength list.
- **Invisible Weather**: generative visualization of the Wi-Fi signals observed during periodic scans (not a packet sniffer, RF spectrum analyzer, or real network traffic meter).
- **Sound Scope**: microphone level visualization.
- **Voice Notes**: button-controlled microphone recordings streamed to your Mac over the USB-C programming connection, then saved as 16 kHz 16-bit mono WAV by the supplied Python helper. No audio files are saved to the M5Stick itself.

## Controls

| Location | Button A | Button B | Hold A |
|---|---|---|---|
| Home | Open selected instrument | Next instrument | — |
| Bluetooth Radar | Device list | All / named-only | Home |
| Bluetooth List | Inspector | Next selected device | Home |
| Inspector | Signal history | Next selected device | Home |
| Signal History | Lost-device alert | Next selected device | Home |
| Lost-device alert | Radar | Arm / disarm selected device | Home |
| Wi-Fi Radar | Networks | Include / exclude hidden networks | Home |
| Wi-Fi Networks | Radar | Next page | Home |
| Invisible Weather | — | Calm / energized visual | Home |
| Sound Scope | Voice Recorder | Clear level | Home |
| Voice Recorder | Sound Scope | Start / stop recording | Home |

Button A is the front button; B is the side button. The voice note records *only when you press B to start and shows an active red REC status*.

## Install firmware (Arduino IDE on Mac)

1. Select **the same ESP32 board and USB port settings that worked with Precision Sonar v2**.
2. In Library Manager install **M5Unified** (version **0.2.22 or newer**), M5GFX (usually pulled in by M5Unified), and **NimBLE-Arduino 2.5.1**.
3. Open `SIGNAL_OS/SIGNAL_OS.ino` and upload.
4. Press **B** on the home screen to cycle apps and **A** to open an app. Hold A to return home.

Wi-Fi Radar and Invisible Weather scan without requiring your SSID or password. The M5StickC Plus only supports 2.4 GHz Wi-Fi, so 5 GHz and 6 GHz access points will not appear. Only access points transmitting beacons are visible; this is *not* a sniffer of personal devices or packets.

## Voice notes → Mac

1. With the M5Stick plugged into USB-C, identify its port under Arduino IDE **Tools → Port** (example `/dev/cu.SLAB_USBtoUART`).
2. Quit Arduino Serial Monitor (it competes for the same serial port).
3. Open Terminal in the extracted `SIGNAL_OS_v3_1_fixed` folder and run:

    ```sh
    python3 -m venv .venv
    .venv/bin/python -m pip install pyserial
    .venv/bin/python record_on_mac.py /dev/cu.SLAB_USBtoUART
    ```

    Substitute the **actual port from your Mac**.
4. On the M5Stick open **VOICE REC**. When `HOST READY` appears, press **B** to record; press **B** to stop.
5. Saved files appear under `SIGNAL_OS_v3_1_fixed/recordings/signal_YYYYMMDD_HHMMSS.wav`.

No audio is uploaded to a cloud service. The Python receiver only writes audio to your Mac.

If `NO RECEIVER` is displayed, check the port, keep the Python script running, and ensure the Arduino Serial Monitor is closed. The receiver uses **921600 baud**.

## Notes / limitations

- **No physical direction tracking**: radial angle is a deterministic visual design choice (address hash), not a bearing.
- **No exact distance**: RSSI varies with orientation, interference and obstacles.
- **Temporary identifiers**: many BLE devices change their addresses, causing them to appear as new devices.
- **Lost-device alarm**: this firmware shows an on-screen alert; buzzer playback is best effort and depends on M5Unified configuration/board. Try it before relying on it for lost-property protection.
- **Audio bandwidth**: USB recording may contain glitches if the serial adapter or Mac cannot sustain the stream; the included receiver does not correct dropped samples. Keep the device plugged in and avoid switching apps while recording.
- **Privacy**: recordings are started intentionally; don't record people without their knowledge and consent where applicable.
- **Power**: active scanning and backlight drain the original Plus's 120 mAh battery quickly. USB power is recommended for extended experiments.
- **Testing**: this project was authored and syntax-checked, but was not compiled with your exact Arduino toolchain or flashed on your board; send any compiler output for a fix.

## References

- https://docs.m5stack.com/en/core/StickC-Plus
- https://github.com/m5stack/M5Unified
- https://github.com/h2zero/NimBLE-Arduino


## Trail Echo — built into SIGNAL OS v3.1

This variant adds **TRAIL ECHO** as home item 6; the existing radio and audio modes are retained.

- Home: B cycles to **6 TRAIL ECHO**; A opens.
- Trail Echo: A starts or stops motion recording; after recording, B switches between waveform and statistics. A starts a new ride.
- Hold A: returns home; any active recording stops and is saved first.
- Trail Echo samples the on-board IMU at a target of 50 Hz and stores one aggregate bin per second.
- If SPIFFS is unavailable, the mode works in RAM but indicates **RAM ONLY**.
- The display dims automatically during recording, returns to normal afterwards.
- No GPS: Trail Echo records **time and relative vibration**, not locations, grades, speed or actual terrain difficulty.

See [TRAIL_ECHO.md](TRAIL_ECHO.md) for architecture, limits and checks.

## MTB companion: Trail Link (planned; protocol scaffold only)

In addition to Trail Echo's ride-vibration recorder, the project now tracks **Trail Link**, a proposed small radio check-in between two nRF24-compatible devices. Its 16-byte packet encoder/decoder and peer freshness tracker are host-tested in `SIGNAL_OS/TrailLinkCore.h` and `tests/test_link_core.cpp`. **No radio driver, new menu screen, or transmitted messages are included.** It requires external radio hardware and a second compatible peer for validation. See [TRAIL_LINK.md](TRAIL_LINK.md) and [issue #12](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/12). This is **not** an emergency device or a LoRa/Meshtastic radio.

## Cardputer ADV GPS Bridge (experimental)

The optional GPS companion is in [`cardputer-adv/`](cardputer-adv/). Existing SIGNAL OS modes run unchanged unless `SIGNAL_OS_TRAIL_BRIDGE_ENABLE` is changed to `1` in `SIGNAL_OS.ino`. The Cardputer sketch logs GNSS to local microSD and receives ESP-NOW motion packets; visualizations export to GeoJSON and SVG. No SX1262 LoRa transmission or RF424 module is used for this bridge. **Arduino/device testing is pending**. See [Cardputer setup](cardputer-adv/README.md) and issue #13.
