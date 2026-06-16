// ============================================================================
//  WifiScanner  -  Non-blocking WiFi AP discovery feeding the NetworkStore.
//  Uses the async scan API so the web UI and AP stay responsive. Coexists with
//  the SoftAP via WIFI_AP_STA mode.
// ============================================================================
#pragma once
#include <Arduino.h>

class WifiScanner {
public:
    void begin();
    void poll();          // call from loop(); schedules + harvests scans
    uint32_t lastScanMs() const { return _lastDone; }

private:
    bool     _scanning = false;
    uint32_t _lastStart = 0;
    uint32_t _lastDone  = 0;
    void harvest();
};

extern WifiScanner g_wifiScanner;
