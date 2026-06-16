// ============================================================================
//  WebInterface  -  Async HTTP UI + JSON/CSV API served from the SoftAP.
// ============================================================================
#pragma once
#include <Arduino.h>

class WebInterface {
public:
    void begin();          // start the AP + web server
    void loop();           // pump the captive-portal DNS server
    String apIp();         // SoftAP IP as string (for the serial banner)

private:
    bool _started = false;
};

extern WebInterface g_web;
