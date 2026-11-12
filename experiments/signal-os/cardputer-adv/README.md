# Trail Echo GPS Bridge / Cardputer ADV

**Implementation candidate, not yet Arduino-compiled or hardware-verified.** This standalone Cardputer ADV sketch logs GNSS from the [M5Stack Cap LoRa-1262](https://docs.m5stack.com/en/cap/Cap_LoRa-1262) and receives 22-byte IMU summaries from the existing M5StickC Plus via native ESP-NOW. The SX1262 LoRa transmitter is **not initialized**. No PINGEQUA RF 424 module is needed.

## Install

1. Attach the cap with power **off**; install the LoRa antenna first, as M5Stack requires even though this sketch doesn't transmit LoRa. Use M2×4mm screws, not longer ones.
2. Insert a **FAT32** microSD in the Cardputer ADV. The sketch prints `NO SD / SERIAL ONLY` if storage cannot mount.
3. In Arduino IDE install **M5Cardputer** / **M5Unified** and **TinyGPSPlus** (M5 fork or compatible), along with current Arduino ESP32 (3.x recommended). Open `TrailEchoGPS/TrailEchoGPS.ino`, target the **Cardputer ADV**, and verify/flash. Use Serial Monitor 115200.
4. Cardputer's **G15 UART RX** reads Cap GPS_TX; **G13 UART TX** goes to Cap GPS_RX, NMEA 115200 8N1. SD bus uses SCK40/MISO39/MOSI14/CS12.
5. The UI shows GNSS fix, SD recording path, ESP-NOW receiver status/channel, packet count, missing sequence count, and packet age. The GPS ceramic antenna needs sky view; wait for a real fix outdoors before trusting coordinates.
6. The M5Stick sketch must be compiled separately with `SIGNAL_OS_TRAIL_BRIDGE_ENABLE` set to `1` in `SIGNAL_OS.ino` to transmit once per completed second of actual Trail Echo recording. This is **OFF by default** so existing SIGNAL OS functionality is preserved without network changes.

## Local data

Cardputer opens a matched pair of new files per boot, such as `G00.CSV` and `I00.CSV`, without overwriting existing ones. GPS file has `local_ms,utc_iso,fix,lat,lon,hdop,satellites`; IMU file has `rx_ms,ride_id,seq,elapsed_s,rms_mg,peak10mg,impacts,tx_ms`. An invalid GNSS fix is recorded with `fix=0` and blank coordinates. RTC timestamps come from the GPS fix, not fabricated device time. Remove the microSD after powering down.

`tools/make_route.py` generates an annotated local GeoJSON file from the two CSV logs. The join uses **Cardputer-local milliseconds** for both streams, so timing does not depend on synchronized MCU clocks. Missing GPS or RF observations are excluded, not interpolated.

## ESP-NOW limitations

- Channel is fixed to **6** on both devices; no AP is required. SIGNAL OS cannot run its Wi-Fi scanner concurrently with the Trail Echo bridge. Entering the Trail Echo view stops the scanner, and leaving it disables the optional bridge.
- The current TX uses **unencrypted broadcast** for motion data only; no GPS locations, messages, authentication, commands, or peer IDs are exchanged. Use for benign tests only and do not assume packet authenticity, reliable delivery or privacy against a nearby receiver.
- Radio send returns `ESP_OK` for **submission**, not proof of reception. A successful 2-device test needs logs from both sides; packet losses and timing jitter must be measured.
- Cardputer's SD and Cap's SX1262 share SPI lines with different CS; this sketch never touches the SX1262 radio. GNSS uses its own UART.
- Avoid pressing buttons/screens while riding; use secure mounts and tethers. This is not a navigation, tracking or rescue device.

## Bench sequence

1. Cardputer alone: confirm the GPS displays `NO FIX` indoors and obtains a valid position outdoors; verify `G00.CSV` has valid/invalid rows.
2. M5Stick alone: confirm all six original modes work **with the feature flag set to 0**.
3. Set flag to 1 and flash M5Stick; enter Trail Echo and record a stationary 30-second test, gently tap/wiggle the stick; keep Cardputer nearby on channel 6.
4. Verify `I00.CSV` has valid unique motion packets with increasing `seq`, `elapsed_s` and plausible `rms_mg`; verify `G00.CSV` has real GNSS positions. Check packet counts and missing sequence gaps.
5. Confirm exiting Trail Echo and returning to Wi-Fi Radar still works and the M5Stick accelerometer cadence stays acceptable; measure power consumption before an actual ride.
6. Execute `python3 tools/make_route.py --gps G00.CSV --imu I00.CSV --output route.geojson` locally. Review dropped rows and that colors correspond only to real matched data.

Hardware differences (especially if the 'Plus 1.2' label refers to a PCB revision rather than **Plus2**) must be checked before flashing. Do not use a Plus2-specific firmware target for an original Plus.

References: [Cap LoRa-1262](https://docs.m5stack.com/en/cap/Cap_LoRa-1262), [Cardputer ADV](https://docs.m5stack.com/en/core/Cardputer-Adv), [Arduino ESP-NOW](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/espnow.html).

## Optional original visual score (SVG)

After creating `route.geojson`, run:

```sh
python3 tools/render_svg.py route.geojson trail-score.svg
```

The image is self-contained, north-up, and uses observed GPS positions only. Adjacent observations are connected *only* if they are from the same ride and separated by at most 2.5 seconds. It does not create a map background or estimate an unobserved route.
