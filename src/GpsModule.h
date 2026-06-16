// ============================================================================
//  GpsModule  -  Optional NMEA GPS support (ATGM336H / NEO-M8N / NEO-6M).
//  The hardware is not required to be present; everything degrades gracefully
//  to "no fix" so the rest of the firmware works today and gains location
//  logging automatically the moment a module is wired up.
// ============================================================================
#pragma once
#include "types.h"

class GpsModule {
public:
    void begin();
    void poll();              // call frequently from loop()
    GpsFix fix();             // thread-safe snapshot of current state
    bool present();           // have we ever received valid NMEA?

private:
    bool   _started = false;
    bool   _present = false;
    GpsFix _fix;
};

extern GpsModule g_gps;
