// ============================================================================
//  BlueLink.h  -  WarDriver's side of the BlueDriver link.
//
//  The companion ESP32-BlueDriver is a headless BLE sensor that POSTs its
//  discovered devices to this board's /ingest endpoint on a timer. This module:
//    * mirrors BlueDriver's reported status (for the BLUEDRIVER tab in the UI),
//    * pushes ingested BLE devices into the shared BLE store (via ingestBleDevice),
//    * holds a queue of control commands that ride back on each POST's response
//      (start/stop/clear scan, active-scan toggle, beacon, GATT enumeration),
//    * keeps the latest GATT report BlueDriver returned.
//
//  The link is single-initiator (BlueDriver -> WarDriver), so the WarDriver
//  never makes a blocking outbound HTTP call from an async handler.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct BlueStatus {
  uint32_t lastSeenMs   = 0;     // millis() of the last /ingest received
  bool     scanning     = false;
  uint32_t devices      = 0;     // BlueDriver's own unique-device count
  uint32_t newDevices   = 0;
  uint32_t uptime       = 0;
  uint32_t heap         = 0;
  uint32_t psram        = 0;
  bool     activeScan   = true;
  bool     beaconActive = false;
  String   beaconInfo;
  bool     gpsValid     = false;
  uint8_t  gpsSats      = 0;
  double   gpsLat       = 0;
  double   gpsLon       = 0;
  uint32_t totalIngested = 0;    // BLE devices accepted into the store here
};

struct BlueCommand {
  uint32_t id;
  String   json;                 // fully-formed object incl. id, e.g.
                                 //   {"id":7,"cmd":"scan","on":0}
};

class BlueLink {
 public:
  void begin();

  // Enqueue a command. `innerFields` is the JSON body WITHOUT the surrounding
  // braces and WITHOUT the id, e.g.  "\"cmd\":\"scan\",\"on\":0".
  // Returns the assigned command id.
  uint32_t enqueue(const String& innerFields);

  // Handle a posted /ingest body: update status, store BLE devices, prune
  // acknowledged commands, and write the response (ack + epoch + commands).
  void handleIngest(const String& body, String& out);

  // Snapshot for GET /api/blue/status.
  void writeStatusJson(String& out);

 private:
  void lock()   { if (mtx_) xSemaphoreTake(mtx_, portMAX_DELAY); }
  void unlock() { if (mtx_) xSemaphoreGive(mtx_); }

  SemaphoreHandle_t        mtx_   = nullptr;
  std::vector<BlueCommand> queue_;
  uint32_t                 nextId_ = 1;
  uint32_t                 epoch_  = 0;   // our boot nonce; resets cmd-id seq
  BlueStatus               st_;
  String                   lastGatt_;     // latest GATT JSON ("" if none)
};

extern BlueLink g_blue;
