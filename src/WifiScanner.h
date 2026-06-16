// ============================================================================
//  WifiScanner  -  Non-blocking WiFi AP discovery feeding the NetworkStore.
//  Scans ONE channel per cycle (rotating 1..WIFI_MAX_CHANNEL) so the SoftAP
//  stays on its home channel almost all the time and clients don't drop.
// ============================================================================
#pragma once
#include <Arduino.h>

class WifiScanner {
public:
    void begin();
    void poll();          // call from loop(); schedules + harvests scans
    bool isScanning() const { return _scanning; }
    uint32_t lastScanMs() const { return _lastDone; }

private:
    bool     _scanning = false;
    uint8_t  _channel   = 1;     // channel currently being rotated through
    uint32_t _lastStart = 0;
    uint32_t _lastDone  = 0;
    void harvest();
};

extern WifiScanner g_wifiScanner;
