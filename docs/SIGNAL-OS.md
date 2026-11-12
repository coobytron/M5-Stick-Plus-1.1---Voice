# SIGNAL OS v3.1 + Trail Echo

Pocket-instrument firmware for the original M5StickC Plus 1.x. This is a **separate firmware experiment** and does not replace the translator or dictation firmware in this repository.

Modes: BLE Radar, Wi-Fi Radar, Invisible Weather, Audio Scope, USB Voice Recorder, and **Trail Echo** (IMU terrain vibration recorder).

**Source:** [`experiments/signal-os/`](../experiments/signal-os/)
**Original concept:** [Trail Echo #8](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/8)
**Setup and controls:** [`experiments/signal-os/README.md`](../experiments/signal-os/README.md)
**Trail Echo technical note:** [`experiments/signal-os/TRAIL_ECHO.md`](../experiments/signal-os/TRAIL_ECHO.md)

## Status

- Imported from the user-provided `SIGNAL_OS_v3_1_fixed(1).zip` and the Trail Echo integration archive generated in ChatGPT.
- Host C++ tests passed for IMU summarization, capacity, impact detection, flash-mode persistence logic using a fake SPIFFS, reboot reload, and RAM-only fallback.
- **Arduino firmware was not compiled using the board toolchain and has not been flashed or physically verified.** Treat this as a working candidate, not a released firmware.
- The later `SIGNAL_OS_v4_Channel_Atlas.zip` is not available in this conversation. **Channel Atlas is not included**; this is v3.1 with Trail Echo integrated.
- No secrets, network passwords, recordings, user-specific hardware credentials, or audio clips are included.

## Host validation

From `experiments/signal-os` run:

```sh
g++ -std=c++11 -Wall -Wextra -pedantic tests/test_core.cpp -o /tmp/trail_echo_core_test && /tmp/trail_echo_core_test
g++ -std=c++11 -Wall -Wextra -pedantic -Itests/fakes tests/test_mode.cpp -o /tmp/trail_echo_mode_test && /tmp/trail_echo_mode_test
```

## Next steps for Claude

Follow [Agent Cody Banks](https://github.com/coobytron/Agent-Cody-Banks) and the repository `AGENTS.md` rules. First compile with the user's Arduino ESP32 / M5Unified / NimBLE 2.5.1 configuration, fix any compatibility issues, and use hardware-validation results before changing design. Preserve the original five modes. For a future v4 merge, require the actual v4 Channel Atlas source and reconcile navigation and data storage carefully. Do not claim a feature is hardware-verified without a recorded test.

## Trail Link (radio companion, not yet enabled)

The MTB partner-check-in concept is tracked in [issue #12](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/12). A 16-byte portable C++11 codec and freshness tracker exist at [TrailLinkCore.h](../experiments/signal-os/SIGNAL_OS/TrailLinkCore.h), with [host tests](../experiments/signal-os/tests/test_link_core.cpp) and a [radio / safety / interaction brief](../experiments/signal-os/TRAIL_LINK.md). **No radio driver, new menu mode, automatic radio transmit, or physical validation is included.** This extension requires the external Stick RF 424 switched to nRF24 at 2.4 GHz and a second matching peer. The existing Trail Echo IMU ride recorder works independently of it. See the separate [host-test workflow](../.github/workflows/signal-os-host.yml). Neither feature is a rescue or emergency system.

## GPS companion: Cardputer ADV + Cap LoRa-1262 (proposed)

The user also owns a **Cardputer ADV with M5Stack Cap LoRa-1262**, which contains an **SX1262 868–923 MHz LoRa transceiver** and an independent **GNSS receiver**. Track the proposed route-aware extension in [issue #13](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/13). **This is a design/test backlog item, not integrated firmware yet.**

Recommended two-device prototype: the M5StickC Plus runs Trail Echo's accelerometer recorder; the Cardputer ADV collects GNSS fixes and a local GPS track. Use the chips' built-in **ESP-NOW / 2.4 GHz Wi-Fi** link to exchange compact, timestamped vibration summaries, with an offline route heat-map generated after the ride. This link needs no extra RF module or access point, but requires matched Wi-Fi channels, real device testing, and accounting for signal loss, GNSS fix validity, and clock differences. Do not let Wi-Fi scans disturb 50 Hz IMU sampling. Keep original SIGNAL OS modes intact.

**Incompatible as shipped:** the Cardputer cap's SX1262 LoRa signal cannot talk over ordinary LoRa to the PINGEQUA RF 424's 2.4 GHz nRF24 or 433 MHz CC1101 radio. Future LoRa-to-LoRa use requires a **second compatible LoRa endpoint** operating on an appropriate band; it is distinct from the short-range ESP-NOW GPS bridge. GNSS logging itself does not require using the cap's LoRa transmitter. Follow the [official Cap manual](https://docs.m5stack.com/en/cap/Cap_LoRa-1262), including the warning to **attach its antenna before powering the module**, even if the intended first experiment is GPS only.

## Trail Echo GPS Bridge — Cardputer ADV implementation candidate

The user owns a **Cardputer ADV with Cap LoRa-1262 (GNSS + SX1262)**. The standalone Cardputer logger is under [`experiments/signal-os/cardputer-adv/`](../experiments/signal-os/cardputer-adv/). It logs satellite fixes at 1 Hz to microSD and receives optional Trail Echo motion summary broadcasts via the *ESP32's built-in* ESP-NOW radio. The GPS data stays on Cardputer; neither SX1262 LoRa nor the PINGEQUA Stick RF 424 is used for this link.

The original M5Stick SIGNAL OS code includes `TrailBridgeWire.h` and `TrailBridgeRadio.h` as an **opt-in only** sender: `SIGNAL_OS_TRAIL_BRIDGE_ENABLE` defaults to **0**, preserving normal Wi-Fi, Bluetooth, audio, and Trail Echo behavior. Once enabled and tested, it transmits one short motion packet per completed second in Trail Echo mode, without GPS or network connection. Unencrypted test-broadcast design is *not* secure or safety-critical.

The Cardputer logger saves paired `Gxx.CSV` and `Ixx.CSV` files (GPS/IMU). `make_route.py` combines real Cardputer-local timestamps into GeoJSON points and excludes missing GPS fixes; `render_svg.py` produces a minimalist self-contained graphic score, with line breaks instead of interpolating lost data. See [GPS Bridge #13](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/13).

**Validation status:** four host C++11 tests and two Python tests pass locally; no compiled Arduino firmware, flashed device, confirmed GNSS fix, confirmed SD writes or confirmed ESP-NOW delivery. The design cannot be called ride-ready until two-board bench and short route tests succeed. Run/compile steps and pin mapping are documented in the Cardputer README.

## RF424 + recording UI prototype

The latest SIGNAL OS candidate on branch `feature/trail-echo-live-record-ui` adds a new RF Atlas, Wi-Fi Channel Atlas, RF Quiet Watch, CC1101-reactive Invisible Weather, a better-lit animated Trail Echo recording screen and real 433 MHz energy overlay when the CC1101 is connected. The RF module is **optional and disabled by default** (`SIGNAL_OS_RF424_CC1101=0`), with no fake signal data in its absence. Its 433 MHz CC1101 RX uses the original StickC Plus 1.x legacy SPI pins G0/G33/G32, CS26, GDO0 25 based on Bruce; the exact PINGEQUA unit and RadioLib build need hardware verification. The physical switch allows only one chipset at once. The nRF24 Trail Link protocol is not a working radio driver; the Cardputer GPS Bridge still uses independent ESP-NOW. See [RF setup, controls, constraints and tests](../experiments/signal-os/README.md#rf424-integration-and-live-trail-echo-ui-prototype-original-plus-1x).

**RadioLib compatibility:** install exactly **RadioLib 7.8.1** for the opt-in CC1101 receiver. RadioLib **8.0.0** (released November 7, 2026) changed `CC1101::begin` and frequency API types; the header intentionally rejects 8.x until its adapter is ported. With the RF flag left at `0`, RadioLib is not required.
