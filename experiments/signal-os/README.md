# SIGNAL OS v3.2 — Universal RF + IR Remote (original M5StickC Plus 1.x)

A small pocket-instrument interface with a graphite and acid-green palette.
**Fix in v3.1:** Renamed custom color constants `WHITE`, `RED`, and `ORANGE` to `UI_WHITE`, `UI_RED`, and `UI_ORANGE` to prevent ambiguity with M5GFX color constants. No intended behavior changes.

Designed for the original **M5StickC Plus / Plus 1.2 (135×240 portrait, 4 MB flash)** — not the Plus2.

## Included

- **Bluetooth**: BLE Radar, named device list, inspector, RSSI history and Lost Alert.
- **Wi-Fi Radar**: 2.4 GHz access-point scan, channel list and RSSI.
- **Channel Atlas**: Wi-Fi AP channel strengths and optional CC1101 signal survey.
- **RF Atlas / Quiet Watch**: 433 MHz signal-level survey and activity-gap view.
- **Trail Echo**: IMU motion-based ride recorder, local storage when SPIFFS is mounted.
- **Universal Remote (new)**: capture/save/replay compatible **fixed-code 433.92 MHz** signals with the PINGEQUA Stick RF 424 CC1101; import Flipper Zero RAW or NEC IR files for transmission through the original Plus's built-in IR LED (GPIO9). See [Universal Remote guide](UNIVERSAL_REMOTE.md).

Removed from this firmware: **Invisible WX, Sound Scope and Voice Record**. These no longer consume menu slots or compile into the sketch.

## Controls

| Location | Button A | Button B | Hold A |
| --- | --- | --- | --- |
| Home | Open selected instrument | Next instrument | — |
| Bluetooth Radar | Device list | All / named-only | Home |
| Bluetooth List / Inspector / History | Next BLE screen | Next device | Home |
| BLE Lost Alert | Radar | Arm / disarm | Home |
| Wi-Fi Radar | Networks | Hidden networks on/off | Home |
| Wi-Fi Networks | Radar | Next page | Home |
| Channel Atlas | Wi-Fi Radar | Display style | Home |
| RF Atlas | RF Quiet Watch | Next frequency | Home |
| RF Quiet Watch | RF Atlas | Arm / disarm | Home |
| Trail Echo | Record / stop | Score / stats; **hold B** to send saved ride to Cardputer | Home |
| Universal Remote | Open selected / learn RF | Next entry | Home |
| RF Learn | Retry capture | Library | Home |
| Saved Remote | Transmit RF or IR once | Library | Home |

The new Home menu contains six instruments (Bluetooth, Wi-Fi Radar, Channel Atlas, RF Atlas, Trail Echo, Universal). The built-in IR diode **cannot learn** codes; use a Flipper Zero .ir import or external IR receiver.

## Install firmware (Arduino IDE on Mac)

1. Select **the same ESP32 board and USB port settings that worked with Precision Sonar v2**.
2. In Library Manager install **M5Unified** (version **0.2.22 or newer**), M5GFX (usually pulled in by M5Unified), **NimBLE-Arduino 2.5.1** and **RadioLib 7.8.1**.
3. Open `SIGNAL_OS/SIGNAL_OS.ino` and upload.
4. Press **B** on the home screen to cycle apps and **A** to open an app. Hold A to return home.

Wi-Fi Radar scans without requiring your SSID or password. The M5StickC Plus only supports 2.4 GHz Wi-Fi, so 5 GHz and 6 GHz access points will not appear. Only access points transmitting beacons are visible; this is *not* a sniffer of personal devices or packets.

## Universal Remote → Flipper import

See [UNIVERSAL_REMOTE.md](UNIVERSAL_REMOTE.md) for RF learning, saved commands, IR import (Python + pyserial), controls, and physical validation checklist.

**Important:** this is the original **M5StickC Plus 1.x** sketch. Its IR transmitter is **GPIO9**, while Plus2 uses GPIO19. The RF424 physical switch must be at **433 MHz** for CC1101 capture/replay.

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


## Trail Echo — built into SIGNAL OS v3.2

This variant includes **TRAIL ECHO** as home item 5; Bluetooth and Wi-Fi remain, while audio modes have been removed.

- Home: B cycles to **5 TRAIL ECHO**; A opens.
- Trail Echo: A starts or stops motion recording; after recording, B switches between waveform and statistics. A starts a new ride.
- Hold A: returns home; any active recording stops and is saved first.
- Hold B on the score or statistics page: sends the saved ride to a Cardputer running TrailEchoGPS (only when `SIGNAL_OS_TRAIL_BRIDGE_ENABLE` is `1`). The bottom line shows `SENDING n%`, then `SENT TWICE`; hold B again to stop (`SEND STOPPED`). See [Send a saved ride](TRAIL_ECHO.md#send-a-saved-ride-after-the-fact).
- Trail Echo samples the on-board IMU at a target of 50 Hz and stores one aggregate bin per second.
- If SPIFFS is unavailable, the mode works in RAM but indicates **RAM ONLY**.
- During recording, an animated pulse and scrolling motion visualization run at brightness 112/255; normal brightness returns when recording stops.
- No GPS: Trail Echo records **time and relative vibration**, not locations, grades, speed or actual terrain difficulty.

See [TRAIL_ECHO.md](TRAIL_ECHO.md) for architecture, limits and checks.

## MTB companion: Trail Link (planned; protocol scaffold only)

In addition to Trail Echo's ride-vibration recorder, the project now tracks **Trail Link**, a proposed small radio check-in between two nRF24-compatible devices. Its 16-byte packet encoder/decoder and peer freshness tracker are host-tested in `SIGNAL_OS/TrailLinkCore.h` and `tests/test_link_core.cpp`. **No radio driver, new menu screen, or transmitted messages are included.** It requires external radio hardware and a second compatible peer for validation. See [TRAIL_LINK.md](TRAIL_LINK.md) and [issue #12](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/12). This is **not** an emergency device or a LoRa/Meshtastic radio.

## Cardputer ADV GPS Bridge (experimental)

The maintained Cardputer firmware is **TrailEchoGPS** in [cardputer-Synth](https://github.com/coobytron/cardputer-Synth/tree/main/TrailEchoGPS) (Launcher `.bin` on its releases; guide in its `docs/trail-echo-gps.md`). It logs live packets and also saves a ride the Stick resends with **B hold** as `/TrailEcho/Snn.CSV`, so a ride recorded while the Cardputer was off or out of range can still be put on the GPS route. The older candidate sketch in [`cardputer-adv/`](cardputer-adv/) does not receive resends.

The original candidate sketch is in [`cardputer-adv/`](cardputer-adv/). Existing SIGNAL OS modes run unchanged unless `SIGNAL_OS_TRAIL_BRIDGE_ENABLE` is changed to `1` in `SIGNAL_OS.ino`. The Cardputer sketch logs GNSS to local microSD and receives ESP-NOW motion packets; visualizations export to GeoJSON and SVG. No SX1262 LoRa transmission or RF424 module is used for this bridge. **Arduino/device testing is pending**. See [Cardputer setup](cardputer-adv/README.md) and issue #13.

## ESP32 IRAM linker overflow: core-version diagnosis

**Observed on 2026-11-11:** Arduino IDE linked this complete SIGNAL OS sketch with the Espressif `esp-x32/2601` toolchain but failed with `.iram0.text will not fit in region iram0_0_seg`, **2,936 bytes** over the IRAM limit. This is *instruction RAM*, not the flash partition, file storage, or heap. Changing “Partition Scheme” or “Flash Size” is not a fix.

**First reproducible workaround:** in Arduino IDE → **Boards Manager**, choose **`esp32` by Espressif Systems, version `3.0.7`**, then select **Tools → Board → esp32 → M5StickCPlus** (original Plus 1.x, **not M5StickCPlus2**) and click **Verify**. Keep `M5Unified`, `M5GFX`, and `NimBLE-Arduino 2.5.1` installed. If the sketch compiles under 3.0.7, this indicates a toolchain/library IRAM budget difference; it does not by itself establish hardware correctness.

**Why this test:** ESP32 core 3.1.x+ has a documented case of an IRAM overrun in a sketch that links under core 3.0.7 with the same libraries. As the Arduino core bundles precompiled ESP-IDF binaries, adding `-DCONFIG_ESP_WIFI_IRAM_OPT=0` in a sketch cannot rebuild those binary libraries; avoid unverified “RAM size” macros or edits to linker memory maps.

**Automated comparison:** [signal-os-arduino.yml](../../.github/workflows/signal-os-arduino.yml) separately compiles this sketch against core 3.0.7 and 3.3.12 and attempts to preserve the build's linker `.map` files to pinpoint IRAM growth. Neither that job's existence nor a pass in one version demonstrates that the other version will fit.

**If 3.0.7 still fails:** share the **full verbose compile log**, board name, installed ESP32 core version and `M5Unified` / `M5GFX` / NimBLE-Arduino versions. In Arduino IDE, File → Preferences → “Show verbose output during compilation”; compile again. We can then inspect the map and identify the IRAM-consuming functions without deleting the BLE, Wi-Fi, Trail Echo or Cardputer bridge features.

**Sources:** [Espressif IRAM troubleshooting](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/ram-usage.html); [Arduino-ESP32 discussion #11062](https://github.com/espressif/arduino-esp32/discussions/11062).

## RF424 integration and live Trail Echo UI (prototype; original Plus 1.x)

SIGNAL OS includes **two new home instruments**: RF Atlas and Channel Atlas. RF Atlas steps through nine 433.2–434.8 MHz tuning points, displaying CC1101 received signal strength. Its A button opens **RF Quiet Watch**, a selectable-frequency activity-gap monitor (B to arm). Channel Atlas displays Wi-Fi channel-derived AP strengths plus available CC1101 readings. Trail Echo shows a live motion-reactive pulse, short rolling motion preview, higher recording brightness (**112/255**, previously 16/255), and RF signal energy when active. Original BLE Lost Alert is unchanged.

**RF 424 configuration:** In v3.2 the compile-time flag `SIGNAL_OS_RF424_CC1101` defaults to **1**, which requires RadioLib **7.8.1**. Set it to **0** for an IR-only build without the RF424 module. RF Atlas has no fabricated readings when the radio is absent.

**Important limits:** signal-strength readings are not packet captures, transmitter identification, direction, distance, radio frequency spectrum occupancy, or proof that a particular device is present. The RF Quiet Watch detects a *period with no above-threshold energy after energy was previously observed*, not the loss of a paired beacon; it cannot be used for safety or emergency alerts. If no energy was seen it displays `WAITING FOR ENERGY` rather than a false lost report. The Universal Remote now offers deliberate, button-initiated 433.92 MHz OOK raw replay for authorized fixed-code devices; RF Atlas itself remains receive-only. RF use will increase battery consumption; on-device detection, UI legibility, RSSI reliability, recording quality and Arduino compilation with RadioLib enabled require physical validation. The user's original Plus 1.x has previously compiled and booted successfully with the correct **M5StickCPlus** board target, but this new revision has not been flashed.

**Related:** [Trail Link #12](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/12), [Trail Echo #8](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/8), [vendor RF 424](https://www.pingequa.com/pages/support-m5stack-stickc-plus-2-in-1-module), [Bruce CC1101 pin mapping](https://github.com/BruceDevices/firmware/wiki/CC1101), [RadioLib CC1101 class](https://jgromes.github.io/RadioLib/class_c_c1101.html).

Portable host regression: `g++ -std=c++11 -Wall -Wextra -Werror -pedantic tests/test_rf_model.cpp -o /tmp/signal_rf_model && /tmp/signal_rf_model`. Run previous Trail Echo, Trail Link and GPS packet host tests unchanged. **The exact physical RF module and Arduino toolchain must still be tested.**

**RadioLib compatibility:** install exactly **RadioLib 7.8.1** for the opt-in CC1101 receiver. RadioLib **8.0.0** (released November 7, 2026) changed `CC1101::begin` and frequency API types; the header intentionally rejects 8.x until its adapter is ported. With the RF flag left at `0`, RadioLib is not required.
