# Universal Remote — SIGNAL OS v3.2

This feature is in **experiments/signal-os/SIGNAL_OS/SIGNAL_OS.ino** for the original **M5StickC Plus 1.x**. It replaces Invisible WX, Sound Scope and Voice Record. BLE Radar, Wi-Fi Radar, Channel Atlas, RF Atlas, Trail Echo and BLE Lost Alert remain.

## Hardware and libraries

- Original M5StickC Plus infrared output: **GPIO 9**. The Plus2 uses GPIO 19 and is not this sketch's target.
- No built-in IR receiver. Capture your own remotes with Flipper Zero and import saved .ir files.
- RF: PINGEQUA RF424 in **433 MHz** switch position, CC1101. SPI mapping retained from prior RF Atlas (CS 26, SCLK 0, MISO 33, MOSI 32, GDO0 25).
- Install M5Unified, M5GFX, **NimBLE-Arduino 2.5.1**, **RadioLib 7.8.1**. RadioLib 8.x is not compatible with the existing adapter.
- Arduino ESP32 core 3.0.7 is the documented starting point for original M5StickCPlus. Watch the IRAM limit (README).
- RF424 is optional for **IR-only** builds if SIGNAL_OS_RF424_CC1101 is set to 0, but enabled by default in this revision.

## Device controls

1. Home: B cycles, A opens **UNIVERSAL**.
2. Remote library: B selects **+ LEARN 433 RF** or a stored remote; A opens.
3. RF learning: press a compatible **fixed-code OOK/ASK 433.92 MHz** remote you own while capture is active. The first plausible frame auto-saves locally. A retries, B returns.
4. Saved remote screen: A transmits the selected RF or IR command; B returns to library.
5. Hold A to return Home.

Storage: SPIFFS, maximum **16 commands × 192 pulse durations**. Remote data persists over reboots, is not uploaded anywhere, and the firmware never formats flash. If storage cannot mount, saves fail visibly. Captures can be noise or incomplete; test against your own receiver.

RF replay is limited to **433.92 MHz raw OOK/ASK-style pulse captures** and makes three repeats for typical fixed-code devices. It does not identify remote brands, handle encrypted/rolling-code remotes or cover all Sub-GHz protocols. The physical slide switch must be at 433 MHz.

## Import Flipper Zero IR files

1. Learn your own IR remote with Flipper Zero and save a .ir file.
2. Transfer the .ir file to your Mac.
3. Plug the M5Stick into USB. Find its port in Arduino IDE → Tools → Port.
4. Install **pyserial**: python3 -m pip install pyserial
5. Import: python3 experiments/signal-os/import_flipper_ir.py --port /dev/cu.usbserial-XXXX ~/Downloads/TV.ir
6. In Signal OS, open UNIVERSAL and select an imported IR command.

For no-hardware validation: python3 experiments/signal-os/import_flipper_ir.py --dry-run ~/Downloads/TV.ir

The importer handles Flipper **raw IR** (30–60 kHz carrier) and **parsed NEC / NECext**. Unsupported parsed protocols, oversized captures, or invalid timings are skipped with an explanation. For these, capture RAW with the Flipper if supported. IR transmitter range is limited by the original Stick's small LED.

## USB Serial management

Connect at **921600 baud**, one newline-terminated command per line.

| Command | Effect |
| --- | --- |
| LIST | Show stored entries (index begins at 0) |
| RENAME\|0\|Desk Lamp | Rename entry zero |
| DELETE\|0 | Delete entry zero |
| IRRAW\|TV Power\|38000\|9000,4500,560,... | Add raw IR pulse timings |

USB commands do **not** transmit RF or IR; pressing physical A on a saved remote is required.

## Physical validation checklist

- [ ] Compile with correct board/core and RadioLib 7.8.1; verify IRAM and flash budget
- [ ] RF424 receiver and Channel Atlas/RF Atlas still work after mode switching
- [ ] Learn and replay a permitted simple 433.92 MHz fixed-code device
- [ ] Verify saved commands survive a reboot; rename and delete entries
- [ ] Import Flipper RAW infrared and NEC parsed commands and test target appliances
- [ ] BLE Radar, Wi-Fi Radar, Trail Echo, lost-device alert and UI are unaffected
- [ ] Test with RF424 disconnected (IR-only build flag disabled)
- [ ] Confirm legal RF band/power and duty-cycle compliance

**No hardware validation has been performed in this branch.** The current implementation is an experimental fixed-code capture/replay and IR-raw transmitter, not a Flipper replacement or emergency control. Use only with authorized devices.
