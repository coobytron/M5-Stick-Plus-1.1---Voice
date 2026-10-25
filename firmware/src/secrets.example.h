// Copy this file to secrets.h (same folder) and fill it in.
// secrets.h is gitignored so your Wi-Fi password never gets committed.
#pragma once

// Up to a few networks; the pen joins whichever is in range when it syncs.
// Your Mac must be on the same network, running mac/receiver.py.
#define WIFI_NETWORKS            \
    {                            \
        {"HomeWiFi", "password"}, \
    }

// Leave empty to find the Mac automatically over Bonjour (_m5dict._tcp).
// Set to the Mac's IP (e.g. "192.168.1.20") if auto-discovery doesn't work.
#define DICTATION_HOST ""
#define DICTATION_PORT 5730
