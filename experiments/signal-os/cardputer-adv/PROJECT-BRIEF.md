# Trail Echo / Cardputer ADV GPS Bridge — Project Brief

- **Owner direction:** begin implementing the proposed Trail Echo GPS Bridge with the owned Cardputer ADV + Cap LoRa-1262 while preserving SIGNAL OS.
- **Outcome:** link real Cardputer GPS fixes with per-second M5Stick IMU vibration and impact measurements and render a local visual trail score.
- **Scope:** standalone Cardputer Arduino GPS microSD logger + ESP-NOW receive; opt-in minimal ESP-NOW sender in the existing SIGNAL OS Trail Echo view; host-testable protocol and offline route/SVG exporter.
- **Explicit non-goals:** SX1262 LoRa transmission, Meshtastic, nRF24 protocol, RF 424 connection, GPS backhaul, cloud upload, guaranteed rider communications, live navigation, or emergency use.
- **Design:** one accountable implementation branch, host test suite, no alteration to existing voice firmware, and default-disabled wireless changes until hardware validation.

## Acceptance gates

- [x] Host wire protocol round trip + invalid packet / CRC / sequence tests.
- [x] GeoJSON builder rejects missing GNSS fixes and gaps.
- [x] SVG exporter renders only real observations and preserves time gaps.
- [x] Existing Trail Echo portable host tests pass.
- [ ] Cardputer ADV firmware compiles on the actual Arduino ESP32/M5Cardputer/TinyGPSPlus toolchain.
- [ ] Real GNSS UART and on-card microSD logs confirmed, including no-fix status.
- [ ] Original M5Stick firmware compiled with feature flag OFF and verified regression-free.
- [ ] Feature flag ON: verified ESP-NOW 2-device delivery / jitter and Wi-Fi mode transitions.
- [ ] Safe off-road / stationary mount test, then an actual ride with separate logs and successful geometry export.
- [ ] Battery use, dropped packets and actual IMU cadence measured; not invented.

## Source hierarchy

1. User instructions and owned hardware.
2. M5Stack [Cardputer ADV](https://docs.m5stack.com/en/core/Cardputer-Adv) and [Cap LoRa-1262](https://docs.m5stack.com/en/cap/Cap_LoRa-1262) vendor pin maps.
3. Espressif [Arduino ESP-NOW](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/espnow.html) API.
4. Existing SIGNAL OS Trail Echo code and host tests.

Follow the repository `AGENTS.md` and [Agent Cody Banks](https://github.com/coobytron/Agent-Cody-Banks). Do not claim hardware validation from host tests.
