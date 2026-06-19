// ============================================================================
//  blue_routes.h  -  WarDriver HTTP routes for the BlueDriver link.
//
//  Registers the /ingest endpoint (where the headless ESP32-BlueDriver POSTs
//  its discovered devices + status, and receives queued control commands on the
//  response) and the /api/blue/* control endpoints used by the BLUEDRIVER tab
//  in the web UI.
//
//  INTEGRATION (only one edit to your existing WebInterface.cpp):
//      #include "blue_routes.h"
//      ...
//      void WebInterface::begin() {
//        ...
//        registerBlueRoutes(s_server);   // <-- add this one line; pass your
//        ...                             //     AsyncWebServer instance
//      }
//
//  registerBlueRoutes() also calls g_blue.begin() for you, so you do NOT need
//  to call it separately.
// ============================================================================
#pragma once
#include <ESPAsyncWebServer.h>

// Register all BlueDriver-link routes on the given server and initialise the
// BlueLink singleton. Call once from WebInterface::begin().
void registerBlueRoutes(AsyncWebServer& server);
