#include "FlockDetector.h"
#include "flock_data.h"
#include <ctype.h>

static bool ouiMatch(const uint8_t* mac, const uint8_t* prefix) {
    return mac[0] == prefix[0] && mac[1] == prefix[1] && mac[2] == prefix[2];
}

// Case-insensitive substring search.
static bool containsCI(const String& haystack, const char* needle) {
    if (!needle || !*needle) return false;
    String h = haystack; h.toLowerCase();
    String n = String(needle); n.toLowerCase();
    return h.indexOf(n) >= 0;
}

namespace FlockDetector {

String vendorHint(const uint8_t* mac) {
    for (size_t i = 0; i < VENDOR_OUIS_COUNT; i++) {
        if (ouiMatch(mac, VENDOR_OUIS[i].prefix)) return VENDOR_OUIS[i].vendor;
    }
    return String();
}

bool classify(const uint8_t* mac, const String& name,
              String& outLabel, String& outVendor) {
    // 1) OUI-based match (highest confidence when the prefix is verified).
    for (size_t i = 0; i < FLOCK_OUIS_COUNT; i++) {
        if (ouiMatch(mac, FLOCK_OUIS[i].prefix)) {
            outLabel  = "FLOCK CAMERA";
            outVendor = FLOCK_OUIS[i].vendor;
            return true;
        }
    }
    // 2) Name/SSID heuristic match.
    for (size_t i = 0; i < FLOCK_NAME_HINTS_COUNT; i++) {
        if (containsCI(name, FLOCK_NAME_HINTS[i])) {
            outLabel  = "FLOCK CAMERA?";   // '?' = heuristic, not OUI-confirmed
            outVendor = "Flock (name match)";
            return true;
        }
    }
    return false;
}

} // namespace FlockDetector
