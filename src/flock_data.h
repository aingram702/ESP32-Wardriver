// ============================================================================
//  flock_data.h  -  Signatures used to flag Flock Safety surveillance cameras.
// ----------------------------------------------------------------------------
//  HONEST CAVEAT (read me):
//  Flock Safety ALPR cameras are primarily solar + LTE devices and do not
//  reliably broadcast WiFi during normal operation, so 100% passive RF
//  detection is not guaranteed. The most dependable signals reported by the
//  community (see the DeFlock project, deflock.me) come from:
//    1. MAC OUI prefixes of the radios used inside the units, and
//    2. WiFi SSIDs / BLE names exposed during install/maintenance modes.
//
//  The lists below are a STARTING POINT and a framework. OUI prefixes change
//  as vendors ship new hardware revisions, so treat these as user-updatable
//  and verify against real field captures before trusting a "DANGEROUS" flag.
//  Add entries you confirm in the field. Each OUI is the first 3 bytes (24
//  bits) of the MAC, written most-significant-byte first.
// ============================================================================
#pragma once
#include <Arduino.h>

struct OuiEntry {
    uint8_t  prefix[3];
    const char* vendor;
};

// --- Candidate OUI prefixes (VERIFY before relying on these) ----------------
// These are seeded as placeholders/examples. Replace or extend with prefixes
// you have personally confirmed belong to Flock hardware in your area.
static const OuiEntry FLOCK_OUIS[] = {
    // { {0x00,0x00,0x00}, "Flock (verify)" },   // <- add confirmed prefixes
};
static const size_t FLOCK_OUIS_COUNT = sizeof(FLOCK_OUIS) / sizeof(OuiEntry);

// --- SSID / BLE-name substrings (case-insensitive) --------------------------
// Heuristic match: if a discovered SSID or BLE advertised name contains one of
// these tokens it is flagged. These catch install/setup beacons and are the
// most useful passive signal in practice. Tune to reduce false positives.
static const char* FLOCK_NAME_HINTS[] = {
    "flock",
    "flocksafety",
    "falcon",        // Flock "Falcon" camera line (may false-positive; tune)
    "penguin",       // reported setup-mode SSID pattern
};
static const size_t FLOCK_NAME_HINTS_COUNT =
    sizeof(FLOCK_NAME_HINTS) / sizeof(char*);

// Generic OUI vendor hints (non-threat) for nicer UI labels. Optional/small.
static const OuiEntry VENDOR_OUIS[] = {
    { {0x3C,0x71,0xBF}, "Espressif" },
    { {0x24,0x6F,0x28}, "Espressif" },
    { {0xDC,0xA6,0x32}, "Raspberry Pi" },
    { {0xB8,0x27,0xEB}, "Raspberry Pi" },
    { {0xAC,0xDE,0x48}, "Apple (private)" },
};
static const size_t VENDOR_OUIS_COUNT = sizeof(VENDOR_OUIS) / sizeof(OuiEntry);
