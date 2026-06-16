// ============================================================================
//  NetworkStore  -  Thread-safe, de-duplicated store of discovered devices.
//  De-dup key is the 48-bit MAC address. Re-seeing a MAC updates the existing
//  entry (RSSI, last-seen time, GPS of strongest hit) instead of adding a row.
// ============================================================================
#pragma once
#include "types.h"
#include <vector>
#include <unordered_map>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class Print;  // forward decl for streaming exporters

class NetworkStore {
public:
    void begin();

    // Insert or update a sighting. `gps` may be an invalid fix. Returns true if
    // this was a brand-new device (useful for LED/notification feedback).
    bool observe(const uint8_t* mac, const String& name, NetType type,
                 int8_t rssi, uint8_t channel, uint8_t encryption,
                 const GpsFix& gps);

    size_t count();
    size_t dangerousCount();
    void   typeCounts(size_t& wifi, size_t& ble);
    void   clear();

    // Stream all devices as a JSON array to `out` (escaped). Caller supplies a
    // Print sink (e.g. AsyncResponseStream).
    void streamJson(Print& out);

    // Stream all devices as RFC-4180 CSV (with CSV-injection guarding).
    void streamCsv(Print& out);

    // Persist / restore the de-duplicated set to a tab-delimited backup file so
    // captures survive a reboot. Independent of the CSV exporter.
    bool save(fs::FS& fsys, const char* path);
    bool load(fs::FS& fsys, const char* path);

private:
    static uint64_t key(const uint8_t* mac);

    std::vector<Device>                 _devices;
    std::unordered_map<uint64_t, size_t> _index;
    SemaphoreHandle_t                   _mtx = nullptr;

    void lock()   { if (_mtx) xSemaphoreTakeRecursive(_mtx, portMAX_DELAY); }
    void unlock() { if (_mtx) xSemaphoreGiveRecursive(_mtx); }
};

extern NetworkStore g_store;
