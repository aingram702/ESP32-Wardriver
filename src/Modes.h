// ============================================================================
//  Modes.h  -  Operating-mode contract shared between main and the web layer.
//  WARDRIVE: WiFi scanning, logging, full web UI. (default)
//  ATTACK:   promiscuous packet monitor + targeted deauth. Scanning paused.
// ============================================================================
#pragma once
#include <Arduino.h>

enum WardriverMode : uint8_t {
    MODE_WARDRIVE = 0,
    MODE_ATTACK   = 1,
};

// Implemented in main.cpp. Switching is centralised so the radio is always
// reconfigured safely when entering/leaving promiscuous mode.
WardriverMode currentMode();
const char*   currentModeName();
void          requestMode(WardriverMode m);
