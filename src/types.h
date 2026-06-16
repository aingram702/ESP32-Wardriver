// ============================================================================
//  types.h  -  Shared data model for discovered devices.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <time.h>

// Network classification used for color-coding in the UI and the CSV "type".
enum NetType : uint8_t {
    NET_WIFI = 0,
    NET_BLE  = 1,
};

// A single de-duplicated discovery. One entry per unique MAC address.
struct Device {
    uint8_t  mac[6]      = {0};
    String   ssid;                 // SSID (WiFi) or advertised name (BLE)
    NetType  type        = NET_WIFI;
    int8_t   bestRssi    = -128;   // strongest signal ever seen
    int8_t   lastRssi    = -128;   // most recent signal
    uint8_t  channel     = 0;      // WiFi channel (0 for BLE)
    uint8_t  encryption  = 0;      // wifi_auth_mode_t value (WiFi only)
    bool     dangerous   = false;  // flagged threat (e.g. Flock camera)
    String   threatLabel;          // e.g. "FLOCK CAMERA"
    String   vendor;               // OUI vendor hint, if known

    uint32_t firstSeenMs = 0;      // millis() at first detection
    uint32_t lastSeenMs  = 0;      // millis() at most recent detection
    time_t   firstSeenUtc = 0;     // UTC epoch if GPS time was available, else 0
    uint16_t hitCount    = 0;      // number of times observed

    // Location of the strongest observation (0 if no GPS fix).
    bool     hasGps      = false;
    double   lat         = 0.0;
    double   lon         = 0.0;
};

// A lightweight snapshot of GPS state, copied under lock when needed.
struct GpsFix {
    bool     valid    = false;
    double   lat      = 0.0;
    double   lon      = 0.0;
    double   altitude = 0.0;
    double   speedKmh = 0.0;
    uint8_t  satellites = 0;
    time_t   utc      = 0;     // 0 if date/time not yet acquired
};

// One captured frame's metadata for the live packet monitor.
struct PacketRecord {
    uint32_t tMs      = 0;
    uint8_t  src[6]   = {0};
    uint8_t  dst[6]   = {0};
    int8_t   rssi     = 0;
    uint8_t  channel  = 0;
    uint8_t  subtype  = 0;     // 802.11 frame subtype
    char     kind[10] = {0};   // "beacon","deauth","data","probe",...
};

// Aggregate packet counters exposed by the sniffer.
struct PacketStats {
    uint32_t total   = 0;
    uint32_t mgmt    = 0;
    uint32_t ctrl    = 0;
    uint32_t data    = 0;
    uint32_t beacon  = 0;
    uint32_t probe   = 0;
    uint32_t deauth  = 0;
    uint32_t eapol   = 0;
};

// Helper: format a MAC into "AA:BB:CC:DD:EE:FF". buf must be >= 18 bytes.
inline void macToStr(const uint8_t* mac, char* buf) {
    snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Helper: parse "AA:BB:CC:DD:EE:FF" -> 6 bytes. Returns true on success.
inline bool strToMac(const String& s, uint8_t* mac) {
    if (s.length() != 17) return false;
    int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        if (v[i] < 0 || v[i] > 0xFF) return false;
        mac[i] = (uint8_t)v[i];
    }
    return true;
}
