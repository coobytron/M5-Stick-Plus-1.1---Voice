# Trail Echo inside SIGNAL OS v3.1

## Scope

This archive was integrated with the exact **SIGNAL_OS_v3_1_fixed** upload. It is **not** based on a v4 Channel Atlas archive, and does **not** add Channel Atlas, which is absent from the provided source. Original BLE / Wi-Fi / Invisible Weather / Audio Scope / USB Voice Recorder modes are preserved.

## How to flash

1. Open `SIGNAL_OS/SIGNAL_OS.ino` from this ZIP in Arduino IDE.
2. Install the same Arduino-ESP32 board support and the same libraries used by your v3.1 sketch: **M5Unified**, **M5GFX**, **NimBLE-Arduino 2.5.1**. Ensure `SignalOS_TrailEcho.h` and `TrailEchoCore.h` stay next to the `.ino`.
3. Select the **original M5StickC Plus 1.x**, not Plus2, and use the same board, flash partition, and upload settings that worked for v3.1. The sketch will *never format* an existing SPIFFS filesystem. If SPIFFS is missing or unusable, recording is RAM-only.
4. Compile (Verify) before uploading. Upload and confirm each previous mode works.

## Control map

| Context | A click | B click | A hold |
|---|---|---|---|
| Home | Open highlighted instrument | Next (six instruments) | Home |
| Trail Echo ready | Start recording | No action | Home |
| Trail Echo recording | Stop + save | Ignored | Stop + save, return Home |
| Trail Echo score | Start new ride | Statistics | Home |
| Trail Echo statistics | Start new ride | Score | Home |

On the score or statistics page, **hold B** to send the saved ride to a Cardputer running TrailEchoGPS (only when `SIGNAL_OS_TRAIL_BRIDGE_ENABLE` is `1`). The bottom line shows `SENDING n%`; hold B again to stop. See *Send a saved ride* below.

Recording screen dims automatically to reduce distraction while riding. Never interact with the display while riding. Secure the enclosure with a robust mount and secondary tether. The accelerometer does not measure location, GPS, actual suspension travel or calibrated trail difficulty.

## Data/persistence

The IMU sampling target is 50 Hz. A slow gravity estimate removes posture; remaining acceleration is quantified into one-second bins. Each bin is four bytes: `rmsMg` (uint16), `peak10Mg` (uint8) and `impactCount` (uint8). Shock threshold ~0.70 g with a 180 ms refractory period. Two-hour hard cap (7200 bins / 28.8 KB RAM); real battery capacity can shorten this substantially.

If SPIFFS mounts, a recording is streamed to `/te-active.bin` and flushed approximately every five seconds. On a normal stop it becomes `/te-last.bin`. The previous completed ride is retained in `/te-recovered.bin` until overwritten by a later ride. After unexpected reboot the active file is salvaged, if possible. Starting a new recording clears the previous **in-memory** ride view but does not delete the completed flash file until a new ride is finalized. A recording stopped with zero completed bins leaves the previous saved ride intact.

If storage fails, the device warns `RAM ONLY` and still records into volatile memory. `SENSOR UNAVAILABLE` prevents starting if M5Unified does not detect the IMU. Sensor dropouts are counted and reported.

## Send a saved ride (after the fact)

Live packets only reach the Cardputer if it was listening during the ride. To capture a ride afterwards, open Trail Echo on the Stick with the ride on screen, put the Cardputer (TrailEchoGPS, from [cardputer-Synth](https://github.com/coobytron/cardputer-Synth)) within a few metres, and hold B. The Stick resends every saved second twice over ESP-NOW channel 6 as kind-2 *replay* packets (`TrailBridgeWire.h`), about 6 ms apart, so a 20-minute ride takes roughly 15 s. The Cardputer saves it as `/TrailEcho/Snn.CSV`. Older receivers ignore replay packets.

The replay carries the ride ID only if the ride was recorded since the Stick was switched on; after a restart the ID is `0`. The route script then needs the ride's start time (`--start-utc`) to place it on the GPS track, because the Stick has no clock. Seconds that never arrive stay missing.

## Tests and caveats

- Pure-C++ test: `g++ -std=c++11 -Wall -Wextra -pedantic tests/test_core.cpp -o /tmp/trail_echo_test && /tmp/trail_echo_test`
- Build requires Arduino-ESP32/M5Unified/NimBLE toolchain. Host C++ tests cannot validate exact Arduino library compatibility or the actual board.
- Basic bench test: six Home items, switch among original modes, short A record and stop, B changes score/stats, long A exits and saves, power-cycle to confirm data, exercise a rough and smooth ride in the same mount, test low battery and filesystem fallback.
- Physical trails may exceed accelerometer range during extreme impacts; mount orientation and stiffness heavily influence relative readings. No GPS, cloud, companion app or SD card is required.
