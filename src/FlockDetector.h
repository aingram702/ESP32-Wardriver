// ============================================================================
//  FlockDetector  -  Decides whether a discovered device is a Flock camera
//  (or other flagged threat) using OUI + name heuristics from flock_data.h.
// ============================================================================
#pragma once
#include "types.h"

namespace FlockDetector {
    // Returns true and fills `label`/`vendor` if the device matches a threat
    // signature. `name` is the SSID (WiFi) or advertised name (BLE).
    bool classify(const uint8_t* mac, const String& name,
                  String& outLabel, String& outVendor);

    // Best-effort vendor lookup for non-threat devices (may return "").
    String vendorHint(const uint8_t* mac);
}
