# SIGNAL OS — App README

**Firmware:** SIGNAL OS v3.2 (Universal Remote feature branch)  
**Target:** original M5StickC Plus 1.x / Plus 1.2 · 135 × 240 portrait display · 4 MB flash  
**Status:** documented from the current firmware source; RF/IR behavior still requires verification on the physical board.

SIGNAL OS is a collection of six pocket instruments for observing nearby wireless signals, recording motion, and testing compatible RF/infrared remote controls. It runs locally on the M5StickC Plus. Some functions require the optional [PINGEQUA Stick RF 424](https://www.pingequa.com/pages/support-m5stack-stickc-plus-2-in-1-module); a separate Cardputer is optional for Trail Echo GPS bridging.

**Start here:** [main Signal OS README](README.md) · [Universal Remote setup](UNIVERSAL_REMOTE.md) · [Trail Echo technical guide](TRAIL_ECHO.md)

## Quick navigation

- **A** = front button; **B** = side button.
- On the home screen, **B** advances through the six apps, and **A** opens the highlighted app.
- **Hold A** returns home from any app. In Trail Echo, leaving during recording stops/saves the ride first.
- **Hold B** has a special action only on a completed Trail Echo ride: send the saved ride to a compatible Cardputer *if the experimental bridge is enabled*.
- Other button actions depend on the current screen; use the tables below.

| Home slot | App | Purpose | Extra hardware |
| --- | --- | --- | --- |
| 1 | [Bluetooth](#1-bluetooth-radar) | Discover, inspect, graph and watch nearby BLE advertisers | None |
| 2 | [Wi-Fi Radar](#2-wi-fi-radar) | Visualize nearby 2.4 GHz access points and their RSSI/channels | None |
| 3 | [Channel Atlas](#3-channel-atlas) | Compare Wi-Fi AP-derived channel strengths and optional 433 MHz RF reading | RF 424 optional |
| 4 | [RF Atlas](#4-rf-atlas--quiet-watch) | Sample nine Sub-GHz frequencies and watch for an activity gap | RF 424, 433 MHz |
| 5 | [Trail Echo](#5-trail-echo) | Record an IMU-based motion signature and inspect a ride score | None; Cardputer optional |
| 6 | [Universal](#6-universal-remote) | Save and transmit authorized 433 MHz fixed-code or IR commands | RF 424 for RF; onboard IR LED for IR |

The six home entries contain more than six screens. For example, Bluetooth has five screens and Universal has a library, RF-learning screen, and per-command detail screen.

---

## 1. Bluetooth Radar

**Home label:** `BLUETOOTH` · **Screen flow:** Radar → Device List → Inspector → RSSI History → Lost Alert → Radar

Uses the ESP32's built-in Bluetooth Low Energy (BLE) scanner to observe nearby advertising devices, even without pairing with them.

### What you see

- **Radar:** circular display of recently observed BLE devices, a live-device count, strongest measured RSSI and a named-only/all-devices filter.
- **Device List:** up to seven visible rows at a time, device names (when broadcast) and RSSI values. The latest 32 device records can be retained in the scanner; items older than approximately 17 seconds disappear from the live list.
- **Inspector:** selected device name, RSSI in dBm, strength bar, elapsed time since last sighting and BLE address.
- **History:** a small, rolling graph of the selected device's signal strength (approximately 36 seconds of samples).
- **Lost Alert:** watches a selected BLE address and indicates when it has been missing for around 12 seconds. A buzzer is attempted when supported by the board/library.

| Screen | A click | B click |
| --- | --- | --- |
| Radar | Open Device List | All devices / named-only |
| Device List | Inspector | Select next device |
| Inspector | RSSI History | Select next device |
| RSSI History | Lost Alert | Select next device |
| Lost Alert | Return to Radar | Arm / disarm the selected device watch |

**Important:** dot angles are decorative and derived from an address hash. They are **not measured bearings**. RSSI is not distance; walls, orientation, interference and random BLE addresses all affect results. The Lost Alert is best-effort and not suitable for security or safety monitoring.

## 2. Wi-Fi Radar

**Home label:** `WI-FI RADAR` · **Screen flow:** Radar ↔ Networks

Scans for nearby **2.4 GHz Wi-Fi access points** without joining a network or requiring a Wi-Fi password. Scans repeat periodically; the display is based on received access-point information, not traffic contents.

### What you see

- **Radar:** approximate signal strength and a dot per detected access point, plus an AP count and strongest RSSI.
- **Networks:** SSIDs, channel numbers, RSSI in dBm and a paginated list of up to eight rows per page. Optionally includes access points with hidden SSIDs.

| Screen | A click | B click |
| --- | --- | --- |
| Wi-Fi Radar | Open Networks | Include / exclude hidden SSIDs |
| Networks | Return to Radar | Next page |

**Important:** this is an **access-point scanner**. It does not decode packets, show devices connected to each network, locate physical transmitters, or scan 5/6 GHz networks. Radar angles are a visual layout choice, not direction measurements.

## 3. Channel Atlas

**Home label:** `CHANNEL ATLAS` · **Screen:** single channel chart

Uses the Wi-Fi access points observed by the Wi-Fi scanner to generate a bar chart for channels **1–13**. The bar heights are aggregated RSSI-derived values from detected access points. A separate line at the bottom reports the peak CC1101 reading when the optional RF 424 is active.

### What you see

- **2.4 GHz / CH:** bars for the observed Wi-Fi channels.
- **433 MHz RF:** reported CC1101 peak signal level in dBm, or `OFF / NO CHIP` when unavailable.
- The chart switches between lime and white bars when you change style.

| A click | B click |
| --- | --- |
| Go to Wi-Fi Radar | Toggle chart contrast/style |

**Important:** the Wi-Fi bars are **not spectrum occupancy, airtime utilization or packet rates**. The 433 MHz row comes from a different receiver, not from the ESP32 Wi-Fi hardware.

## 4. RF Atlas + Quiet Watch

**Home label:** `RF ATLAS` · **Screen flow:** RF Atlas ↔ RF Quiet Watch

Requires the RF 424's **CC1101** side, with its physical switch at **433 MHz** and the build flag `SIGNAL_OS_RF424_CC1101=1`. The firmware supports **RadioLib 7.8.1** for this integration.

### RF Atlas

- Samples **nine center frequencies**, starting at **433.20 MHz** and ending at **434.80 MHz**, in **0.20 MHz** steps.
- Presents a bar per tuning point plus a peak RSSI reading in dBm.
- Highlights the selected frequency used by the Quiet Watch.
- Shows `MODULE OFF` / `RF424 NOT ACTIVE` when the receiver is unavailable; it does not invent measurements.

### RF Quiet Watch

- Displays the selected center frequency and whether the watch is armed.
- Uses a threshold of approximately **−75 dBm** in the current implementation.
- First requires observing above-threshold energy on the selected frequency. If none has been seen, the display says `WAITING FOR ENERGY`.
- After an observed above-threshold signal is no longer seen for **12 seconds**, it shows `QUIET FOR 12s+`.

| Screen | A click | B click |
| --- | --- | --- |
| RF Atlas | Open Quiet Watch | Select next frequency; disarm watch |
| RF Quiet Watch | Return to RF Atlas | Arm / disarm |

**Important:** RF Atlas is a **narrowband, time-stepped RSSI sampler**, not an SDR waterfall or protocol decoder. Quiet Watch monitors an activity gap, **not a paired device's presence**. It cannot identify the transmitter, measure direction or distance, or provide dependable lost-person/equipment alerts.

## 5. Trail Echo

**Home label:** `TRAIL ECHO` · **Screen flow:** Ready → Recording → Score ↔ Statistics

Uses the M5Stick's built-in inertial measurement unit (IMU) to build a motion/vibration signature. It is intended for relative comparisons of rides or surfaces, not for true location tracking.

### What you see

- **Ready:** “Terrain Memory” prompt, storage availability and start instruction.
- **Recording:** an animated pulse, motion level in milligravity (mg), a short live history and elapsed time. With a working RF424, the screen can also show 433 MHz RSSI.
- **Score:** a timeline of relative vibration intensity and marked impacts, scaled to the recorded ride.
- **Statistics:** duration, average RMS acceleration, peak RMS acceleration, impact count and any sensor-read errors.

| State | A click | B click | B hold |
| --- | --- | --- | --- |
| Ready | Begin recording | No change | No action |
| Recording | Stop and save | No change | No action |
| Completed score | Start a new recording | Switch to statistics | Send saved ride to Cardputer (optional bridge) |
| Completed statistics | Start a new recording | Switch to score | Send saved ride to Cardputer (optional bridge) |

### Storage and Cardputer connection

- Targets **50 IMU samples per second** and stores one compact aggregate per second, with a nominal two-hour maximum.
- When SPIFFS is mounted, saves the ride to local flash; when unavailable, the app still runs in **RAM ONLY** mode and data will not persist after shutdown.
- The optional `SIGNAL_OS_TRAIL_BRIDGE_ENABLE=1` build path uses ESP-NOW to send motion data to a listening compatible Cardputer.
- After a completed ride, **hold B** to replay its saved data to [TrailEchoGPS in cardputer-Synth](https://github.com/coobytron/cardputer-Synth/tree/main/TrailEchoGPS). A progress indicator says `SENDING n%` and can report `SENT TWICE`; another B hold can stop a replay. With the bridge disabled (the default), the display can report `BRIDGE OFF`.

**Important:** it measures **relative motion**, not position, speed, grade, GPS, suspension travel, or a scientifically calibrated trail rating. Results depend on how the Stick is mounted. Secure it before biking, and do not operate it while riding.

For detailed data formats and replay workflow, see [TRAIL_ECHO.md](TRAIL_ECHO.md).

## 6. Universal Remote

**Home label:** `UNIVERSAL` · **Screen flow:** Library → RF Learn **or** Saved Command

Combines two different outputs:

- **RF:** a 433.92 MHz CC1101 capture/replay experiment for simple, compatible **fixed-code OOK/ASK** remotes you own or are authorized to test.
- **IR:** transmit imported infrared pulse timings using the original M5StickC Plus's built-in **GPIO 9 IR LED**.

### What you see

- **Library:** saved commands labeled `RF` or `IR`, plus `+ LEARN 433 RF`.
- **RF Learn:** prompts you to press a compatible 433.92 MHz remote. A plausible burst is automatically captured and saved if storage is available, then shown in the command-detail view.
- **Saved Command:** command name, type, pulse count, waveform preview and explicit transmit action.

| Screen | A click | B click |
| --- | --- | --- |
| Remote Library | Open selected command or `+ LEARN 433 RF` | Select next item |
| RF Learn | Retry listening | Return to Library |
| Saved Command | Transmit selected RF/IR command | Return to Library |

### Capture, storage and IR import

- RF learning listens at **433.92 MHz**, not all Sub-GHz bands. It uses the CC1101's raw pulse transitions rather than an automatic protocol decoder.
- Commands live locally in SPIFFS; the library supports at most **16 commands**, each containing at most **192 pulse durations**.
- The device **cannot learn arbitrary IR signals directly**, because this model has an IR LED but no built-in IR receiver.
- Use a Flipper Zero to capture an IR remote you own, transfer its `.ir` file to a computer and import via the included `import_flipper_ir.py` USB serial helper. This importer supports Flipper **raw IR** and parsed **NEC / NECext**; not every IR protocol or very long capture is supported.
- Library management over USB serial includes `LIST`, `RENAME`, `DELETE` and `IRRAW`. Serial commands do not transmit; sending requires **A** on the Stick.

**Important:** compatible fixed-code replay is not the same as defeating rolling-code or encrypted devices. A saved frame may be noise, incomplete, or incompatible. Transmit only to your own or explicitly authorized equipment, respecting applicable RF rules. Onboard IR output and CC1101 capture/replay remain **hardware-unverified** in this feature branch.

For wiring, libraries, the Flipper import command and troubleshooting, see [UNIVERSAL_REMOTE.md](UNIVERSAL_REMOTE.md).

---

## Hardware and setup

| Item | Needed for | Notes |
| --- | --- | --- |
| Original M5StickC Plus 1.x / 1.2 | All six apps | This firmware does **not** target M5StickC Plus2 |
| Built-in ESP32 BLE + 2.4 GHz Wi-Fi | Bluetooth, Wi-Fi Radar, Channel Atlas | No external radio or Wi-Fi credentials required |
| Built-in IMU | Trail Echo | Must be detected by M5Unified |
| Built-in IR LED, GPIO 9 | Universal infrared transmit | Does **not** receive infrared commands |
| PINGEQUA RF 424, CC1101 | RF Atlas, RF Quiet Watch, RF readout in Channel Atlas/Trail Echo, Universal RF | Move module switch to **433 MHz**; only one RF424 radio is selected at a time |
| Optional Flipper Zero | IR command acquisition | Export captured IR as a `.ir` file for the serial importer |
| Optional Cardputer running TrailEchoGPS | Trail Echo GPS pairing and saved-ride replay | Requires experimental bridge build flag and compatible receiver |

**Arduino dependencies:** M5Unified, M5GFX, NimBLE-Arduino **2.5.1** and, for RF424-enabled builds, RadioLib **7.8.1**. Use the original `M5StickCPlus` board target. ESP32 Arduino core **3.0.7** is the documented starting point for investigating an existing IRAM linker-budget issue; always compile for your installed toolchain before flashing.

### Compile-time features

| Flag | Current default | Effect |
| --- | --- | --- |
| `SIGNAL_OS_RF424_CC1101` | `1` | Enables CC1101 dependency and RF code. Set to `0` for an IR-only firmware build |
| `SIGNAL_OS_TRAIL_BRIDGE_ENABLE` | `0` | Leaves the experimental ESP-NOW Trail Echo / Cardputer bridge disabled unless explicitly enabled |

For exact pin mapping and build caveats, use [README.md](README.md) and [UNIVERSAL_REMOTE.md](UNIVERSAL_REMOTE.md).

## What was removed in v3.2

**Invisible WX**, **Sound Scope**, and **Voice Record** are removed from the active home menu and sketch to make room for the combined Universal Remote. Their older documents or helper scripts in the repository do **not** mean these are still available in this firmware build. The earlier root-level translator/dictation firmware is a **separate project entry point**, not a Signal OS app.

## Known limitations / validation

- The firmware's **BLE and Wi-Fi radars are signal visualizations**, not directional radars or spectrum analyzers.
- **RF Atlas** measures CC1101 RSSI; **Universal Remote RF** records/replays compatible raw commands. They are different functions sharing one radio.
- RF424's nRF24L01+ **2.4 GHz** side is not used as a currently available home-screen app. **Trail Link** is a protocol scaffold, not a deployed on-device nRF24 messenger.
- Lack of a module, storage mount or compatible protocol is shown as unavailable or limited rather than being silently simulated.
- Static/host test results do not prove that captured RF/IR signals will work with real remotes. Validate receiver performance, GPIO pin wiring, IR LED output, RF transmit timing, flash/IRAM size and the remaining apps on the physical board before considering the remote feature complete.

### Source of truth

- Firmware and all menu routing: [SIGNAL_OS/SIGNAL_OS.ino](SIGNAL_OS/SIGNAL_OS.ino)
- RF sampler and watch model: [SignalOS_RF424.h](SIGNAL_OS/SignalOS_RF424.h), [SignalOS_RFModel.h](SIGNAL_OS/SignalOS_RFModel.h)
- Remote capture/IR/storage: [SignalOS_UniversalRemote.h](SIGNAL_OS/SignalOS_UniversalRemote.h)
- Trail Echo storage/data: [SignalOS_TrailEcho.h](SIGNAL_OS/SignalOS_TrailEcho.h), [TRAIL_ECHO.md](TRAIL_ECHO.md)
- Optional Cardputer replay: [TrailBridgeRadio.h](SIGNAL_OS/TrailBridgeRadio.h), [TRAIL_LINK.md](TRAIL_LINK.md)

*Scope note: this manual describes the v3.2 Universal Remote feature branch, not the unchanged `main` firmware until the pull request is merged.*
