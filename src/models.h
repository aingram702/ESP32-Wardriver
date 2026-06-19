// ===========================================================================
//  models.h  —  Shared data structures for scanned WiFi / BLE devices.
// ===========================================================================
#pragma once
#include <Arduino.h>

// Severity used by the rogue-AP detector and surfaced in the dashboard.
enum Severity : uint8_t { SEV_NONE = 0, SEV_WATCH = 1, SEV_ALERT = 2 };

struct WiFiAP {
    String   ssid;            // network name ("" => hidden)
    String   bssid;           // AP MAC, lowercase hex "aa:bb:cc:dd:ee:ff"
    uint8_t  bssidRaw[6] = {0};
    int32_t  rssi      = 0;   // dBm
    int32_t  rssiBest  = -127;
    uint8_t  channel   = 0;
    String   encryption;      // OPEN / WEP / WPA / WPA2 / WPA3 / WPA2-ENT ...
    String   vendor;          // OUI vendor or "Randomized/Local"
    bool     hidden    = false;
    uint32_t firstSeenMs = 0;
    uint32_t lastSeenMs  = 0;
    uint32_t timesSeen   = 0;
    // Rogue analysis
    Severity severity  = SEV_NONE;
    String   reason;          // human-readable explanation when flagged
};

struct BLEDev {
    String   address;         // device MAC / address
    String   name;            // advertised name ("" if none)
    int32_t  rssi      = 0;
    int32_t  rssiBest  = -127;
    String   vendor;          // OUI vendor (for public addresses)
    String   addrType;        // public / random
    int      appearance = -1;
    String   manufacturer;    // company id (first 2 bytes of mfg data), hex
    String   services;        // comma-separated advertised service UUIDs
    uint32_t firstSeenMs = 0;
    uint32_t lastSeenMs  = 0;
    uint32_t timesSeen   = 0;
};

// ---- Helpers ---------------------------------------------------------------
inline String macToStr(const uint8_t* m) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             m[0], m[1], m[2], m[3], m[4], m[5]);
    return String(buf);
}

// Map dBm to a 0-100 "quality" percentage for the signal bars in the UI.
inline int rssiToQuality(int32_t rssi) {
    if (rssi <= -100) return 0;
    if (rssi >= -50)  return 100;
    return 2 * (rssi + 100);
}

// Implemented in main.cpp. Merge a BLE device reported by the companion
// ESP32-BlueDriver (received on POST /ingest) into the shared BLE store.
// Thread-safe (takes the data mutex internally). Returns true if newly added.
bool ingestBleDevice(const String& mac, const String& name,
                     const String& vendor, int rssi, uint32_t hits);
