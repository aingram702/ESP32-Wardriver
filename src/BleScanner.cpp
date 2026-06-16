#include "BleScanner.h"
#include "config.h"
#include "NetworkStore.h"
#include "GpsModule.h"
#include <NimBLEDevice.h>

BleScanner g_bleScanner;
static volatile bool s_scanDone = false;

// Callback invoked for every advertisement received during a scan.
class BleAdvCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        uint8_t mac[6];
        // NimBLEAddress::toString() -> "aa:bb:cc:dd:ee:ff"
        String addr = String(dev->getAddress().toString().c_str());
        addr.toUpperCase();
        if (!strToMac(addr, mac)) return;

        String name;
        if (dev->haveName()) name = String(dev->getName().c_str());

        GpsFix gps = g_gps.fix();
        g_store.observe(mac, name, NET_BLE, (int8_t)dev->getRSSI(),
                        0 /*channel n/a*/, 0 /*enc n/a*/, gps);
    }
};

static void onScanComplete(NimBLEScanResults results) {
    s_scanDone = true;
}

void BleScanner::begin() {
    if (_inited) return;
    NimBLEDevice::init("");                     // empty name, scan-only
    NimBLEScan* scan = NimBLEDevice::getScan();
    // Heap-allocate so NimBLE may safely free it on deinit() (it owns the ptr).
    scan->setAdvertisedDeviceCallbacks(new BleAdvCallbacks(), /*wantDuplicates=*/false);
    scan->setActiveScan(true);                  // request scan responses (names)
    scan->setInterval(100);
    scan->setWindow(99);
    scan->setMaxResults(0);                     // 0 = don't buffer, use callback
    _inited = true;
    Serial.println("[BLE] NimBLE scanner ready");
}

void BleScanner::poll() {
    if (!_inited) return;
    uint32_t now = millis();

    if (_scanning) {
        if (s_scanDone) _scanning = false;
        return;
    }
    if (now - _lastStart >= BLE_SCAN_INTERVAL_MS) {
        _lastStart  = now;
        s_scanDone  = false;
        _scanning   = true;
        NimBLEDevice::getScan()->start(BLE_SCAN_DURATION_S, onScanComplete, false);
    }
}

void BleScanner::stop() {
    if (!_inited) return;
    NimBLEDevice::getScan()->stop();
    NimBLEDevice::deinit(true);     // free the controller for promiscuous WiFi
    _inited   = false;
    _scanning = false;
}
