# Trail Link — MTB peer check-ins (optional RF experiment)

**Status:** host-tested packet protocol only; **no RF transmission, M5Stick Arduino compilation, or real-hardware test has happened**. Trail Echo is already implemented independently in SIGNAL OS v3.1; Trail Link is a future companion, **not a replacement**. Tracking: [issue #12](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/12).

## The idea

Mount one M5Stick to each bike, or give one to a riding partner. At a stop, pick a compact check-in (`PING`, `OK`, `WAIT`, `MEET`) and transmit it. On the partner's unit the display indicates the last **received** message and how long ago it was received. If nothing has been heard recently, show `NO RECENT PACKET`, **not** `RIDER LOST`, `NEARBY`, or an invented distance.

This feature is for **non-critical social signaling**. Do not use it for emergencies, rescue, tracking riders, guaranteed communication, or navigating. Do not interact with the screen or buttons while riding.

## Hardware and feasibility

- **Built in:** M5StickC Plus 1.x TFT / buttons / CPU / battery. The ESP32's Wi-Fi/BLE radios are *not* nRF24 or LoRa transmitters.
- **Optional required for nRF24 transport:** PINGEQUA **Stick RF 424** with its mechanical switch on the `2.4 GHz` / `nRF24L01+` side. One radio-equipped peer device is required at **each end**, i.e., two compatible nRF24 radio endpoints. The vendor documents the module with **Bruce** firmware; it does not certify this SIGNAL OS sketch. See [module](https://github.com/pingequalab/stick-rf-424) and [vendor setup](https://www.pingequa.com/pages/wiki).
- **Not compatible:** LILYGO 915 MHz **LoRa/Meshtastic** radios cannot decode nRF24 packets. CC1101 433 MHz hardware mode is *not* selected by this proposal; do not assume you can use 915 MHz LoRa firmware with the 433 MHz hardware.
- **Unresolved before any radio driver:** confirm the exact original Plus 1.x + RF 424 SPI SCK/MISO/MOSI, CSN and CE pins, power, plug orientation, switch behavior and any conflicts with M5Unified. Do not copy the distinct Plus2 pin map or assume generic RF24 example pins.
- Range and battery performance **must be physically measured** in the actual mounts. No promised range, GPS coordinates, peer distance, or heading.

## Minimal interaction direction

A single information-dense, calm page. Show a four-letter preset, large enough to read while **stopped**. Keep transmission controls distinct from the Trail Echo recording buttons by putting them on separate pages. The peer card displays `LAST: OK`, `AGE: 12s`, and `PEER BAT: 73%` only when those values came from valid packets. When stale, show `NO RECENT PACKET`. No fake signal-strength bars: the nRF24L01+ does not provide calibrated RSSI.

### Proposed controls (not yet in firmware)

| State | A click | B click | A hold |
|---|---|---|---|
| Trail Link | Send selected preset | Cycle `PING/OK/WAIT/MEET` | Home |

Optionally send a low-frequency periodic PING while on this page **only after** confirming radio/IMU scheduling, and only while the radio is configured. The user should be able to disable radio transmission entirely.

## Protocol artifact (implemented and host-tested)

`SIGNAL_OS/TrailLinkCore.h` contains the zero-dependency wire encoder, decoder, and peer tracker. It compiles with C++11 and can run in a future nRF24 adapter, but **no adapter is present** in the initial integration. All integers are little-endian; exactly 16 bytes:

| Bytes | Contents |
|---|---|
| 0–1 | `TL` magic |
| 2 | Version `1` |
| 3 | Kind: 1 `PING`, 2 `OK`, 3 `WAIT`, 4 `MEET` |
| 4–7 | 32-bit sender ID (pseudonymous, configured/locally generated) |
| 8–9 | 16-bit increasing sequence |
| 10–11 | Sender uptime in seconds, modulo 65536 |
| 12 | Battery percentage 0–100, or 255 when unknown |
| 13 | Reserved zero |
| 14–15 | CRC16/CCITT-FALSE over bytes 0–13 |

The tracker accepts only packets for an explicitly configured peer ID, rejects duplicates/older sequence values while recently connected, handles 16-bit sequence wrap, and expires the status after 30 seconds. After 60 seconds without packets, sequence tracking resets so a rebooted device can rejoin. `millis()` overflow is supported by unsigned subtraction.

**Important security caveats:** CRC is only accidental-corruption detection; packets are **neither encrypted nor authenticated**. Sender ID is not proof of identity; a nearby device can impersonate it. There is no guaranteed delivery or historical message log, and timing can become ambiguous around reboots. This protocol cannot be used as a safety or emergency service.

## Validation

Host regression from this folder:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic tests/test_link_core.cpp -o /tmp/trail_link_test && /tmp/trail_link_test
```

The current `test_link_core.cpp` covers byte order, round-trip, CRC corruption, truncated/version/invalid fields, unknown sender, duplicate/out-of-order messages, counter rollover, staleness and timer rollover.

### Remaining hardware acceptance checks

1. Inspect actual module pins with documentation or continuity measurements. Establish pin map before configuring `RF24`/SPI.
2. Make a minimal independent firmware experiment, with both ends using the same address/channel and independently generated sender IDs. Confirm the radio can receive and transmit **between two real radios** on the 2.4 GHz side.
3. Test wrong switch, absent hardware, peer turned off and resets; never fabricate a received packet or report success without a valid response.
4. Build a fully optional SIGNAL OS page only after the hardware test, preserving all existing modes. Measure IMU sampling quality and battery life if Trail Echo and Trail Link operate together.
5. Compare packet reception in a safe, stationary outdoor range test with the exact intended antenna orientations and enclosure. **Never depend on it for rider safety.**

## Source hierarchy and handoff

- User intent: optional MTB check-in companion to existing Trail Echo.
- This note + [issue #12](https://github.com/coobytron/M5-Stick-Plus-1.1---Voice/issues/12) are the protocol/interaction brief.
- Source code is an isolated, *unconnected* library; **not a new menu instrument yet**.
- Claude: follow `AGENTS.md`, preserve the voice firmware and five original SIGNAL OS instruments, and do not reinterpret the Art Director / Creative Technologist / Industrial Designer checklists as executable agents. The first unsolved task is **actual radio-pin / two-device proof**, not a polished screen.
