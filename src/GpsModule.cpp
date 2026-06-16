#include "GpsModule.h"
#include "config.h"
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

GpsModule g_gps;

static TinyGPSPlus  s_gps;
static HardwareSerial s_serial(GPS_UART_NUM);

void GpsModule::begin() {
#if GPS_ENABLED
    s_serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    _started = true;
    Serial.printf("[GPS] UART%d @ %d baud (RX=%d TX=%d) - waiting for fix\n",
                  GPS_UART_NUM, GPS_BAUD, GPS_RX_PIN, GPS_TX_PIN);
#else
    Serial.println("[GPS] disabled in config.h");
#endif
}

void GpsModule::poll() {
    if (!_started) return;

    while (s_serial.available()) {
        if (s_gps.encode(s_serial.read())) {
            _present = true;
        }
    }

    // Refresh our snapshot from whatever TinyGPS has decoded.
    GpsFix f;
    f.satellites = s_gps.satellites.isValid() ? s_gps.satellites.value() : 0;

    if (s_gps.location.isValid() && s_gps.location.age() < 5000) {
        f.valid = true;
        f.lat   = s_gps.location.lat();
        f.lon   = s_gps.location.lng();
        if (s_gps.altitude.isValid()) f.altitude = s_gps.altitude.meters();
        if (s_gps.speed.isValid())    f.speedKmh = s_gps.speed.kmph();
    }

    // Build a UTC epoch if we have both a valid date and time.
    if (s_gps.date.isValid() && s_gps.time.isValid() && s_gps.date.year() >= 2020) {
        struct tm t = {};
        t.tm_year = s_gps.date.year() - 1900;
        t.tm_mon  = s_gps.date.month() - 1;
        t.tm_mday = s_gps.date.day();
        t.tm_hour = s_gps.time.hour();
        t.tm_min  = s_gps.time.minute();
        t.tm_sec  = s_gps.time.second();
        // timegm-equivalent: interpret as UTC.
        f.utc = mktime(&t);  // device TZ defaults to UTC (no setenv), good enough
    }

    _fix = f;
}

GpsFix GpsModule::fix()  { return _fix; }
bool   GpsModule::present() { return _present; }
