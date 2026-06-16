// ============================================================================
//  BleScanner  -  BLE device discovery via NimBLE, feeding the NetworkStore.
//  Runs periodic non-blocking active scans. Coexists with WiFi through the
//  ESP-IDF software-coexistence layer.
// ============================================================================
#pragma once
#include <Arduino.h>

class BleScanner {
public:
    void begin();
    void poll();         // call from loop(); schedules scans
    void stop();         // release the controller (used when entering sniff mode)

private:
    bool     _inited   = false;
    bool     _scanning = false;
    uint32_t _lastStart = 0;
};

extern BleScanner g_bleScanner;
