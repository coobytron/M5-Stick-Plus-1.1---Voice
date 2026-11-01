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
