# Compatibility and code status

- **Cardputer ADV** is ESP32-S3 (not original Cardputer firmware interchangeable). M5Stack official pins are used; target Arduino-ESP32 3.x. M5Cardputer library and TinyGPSPlus must be installed in IDE.
- **Cap LoRa-1262** GNSS is separate from SX1262 LoRa and accessible on G15 RX / G13 TX @ 115200; supports NMEA. Onboard microSD shares SPI with SX1262, but the latter is not initialized. Do not omit required physical LoRa antenna.
- **M5StickC Plus** original ESP32 (not Plus2). The existing M5Unified app remains unchanged when `SIGNAL_OS_TRAIL_BRIDGE_ENABLE 0`; optional ESP-NOW broadcast enabled only when set to `1`. ESP-NOW on existing Wi-Fi scanning view is not simultaneous.
- **Not run on hardware.** Host protocol and route tests cannot prove SD, GNSS or wireless functions. Check compile logs and board behavior prior to claiming success.
