#include "WifiScanner.h"
#include "config.h"
#include "NetworkStore.h"
#include "GpsModule.h"
#include <WiFi.h>

WifiScanner g_wifiScanner;

void WifiScanner::begin() {
    // Scanning needs STA enabled alongside the SoftAP.
    WiFi.mode(WIFI_AP_STA);
    // Disable WiFi modem power-save: critical for a responsive, stable SoftAP.
    WiFi.setSleep(false);
}

void WifiScanner::poll() {
    uint32_t now = millis();

    if (!_scanning) {
        if (now - _lastStart >= WIFI_SCAN_INTERVAL_MS) {
            _lastStart = now;
            // Scan a SINGLE channel (async). Passive scanning only listens for
            // beacons, so the radio leaves the AP channel for ~one dwell window
            // rather than sweeping every channel for >1 s.
            int rc = WiFi.scanNetworks(/*async*/true, /*hidden*/true,
                                       /*passive*/WIFI_SCAN_PASSIVE,
                                       WIFI_SCAN_PER_CH_MS, _channel);
            if (rc == WIFI_SCAN_RUNNING || rc == WIFI_SCAN_FAILED) {
                _scanning = true;
            }
        }
        return;
    }

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;     // still going
    if (n == WIFI_SCAN_FAILED)  { _scanning = false; WiFi.scanDelete(); return; }

    harvest();
    _scanning = false;
    _lastDone = now;
    WiFi.scanDelete();

    // Advance to the next channel for the following cycle.
    if (++_channel > WIFI_MAX_CHANNEL) _channel = 1;
}

void WifiScanner::harvest() {
    int n = WiFi.scanComplete();
    if (n <= 0) return;

    GpsFix gps = g_gps.fix();
    for (int i = 0; i < n; i++) {
        const uint8_t* bssid = WiFi.BSSID(i);
        if (!bssid) continue;
        g_store.observe(bssid,
                        WiFi.SSID(i),
                        NET_WIFI,
                        (int8_t)WiFi.RSSI(i),
                        (uint8_t)WiFi.channel(i),
                        (uint8_t)WiFi.encryptionType(i),
                        gps);
    }
}
