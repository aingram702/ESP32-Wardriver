# WarDriver `/ingest` protocol (BlueDriver link)

As of BlueDriver firmware **2.0.0**, the BlueDriver is a *headless BLE sensor*.
It has no Wi-Fi AP and no web UI. It joins the WarDriver's SoftAP as a station
and POSTs to `http://192.168.4.1/ingest` on a timer (default every 4 s). The
same request both **uploads BLE devices + status** and **pulls queued control
commands** back on the response. This is the only connection between the two
boards, and it is always initiated by BlueDriver — so the WarDriver never makes
a blocking outbound HTTP call from inside an async handler.

## Request — `POST /ingest`

`Content-Type: application/json`

```jsonc
{
  "source": "ESP32-BlueDriver",
  "ver": "2.0.0",
  "linkEpoch": 0,            // WarDriver boot nonce BlueDriver currently believes
  "lastCmdId": 0,            // highest command id BlueDriver has executed
  "status": {
    "scanning": 1,
    "devices": 1234,         // BlueDriver's own unique-device count
    "newDevices": 12,        // not-yet-uploaded count
    "uptime": 3600,          // seconds
    "heap": 210000,          // free heap bytes
    "psram": 8000000,        // free PSRAM bytes
    "activeScan": 1,
    "beaconActive": 0,
    "beaconInfo": ""         // human-readable beacon description, "" if off
  },
  "gps": { "valid": 1, "sats": 9, "lat": 39.1, "lon": -84.6 },
  "gattResult": { /* present only when a prior gatt command produced output */ },
  "count": 2,
  "devices": [
    {
      "mac": "aa:bb:cc:dd:ee:ff",
      "name": "Tile",
      "vendor": "Tile, Inc.",
      "rssiBest": -61,        // (or "rssi")
      "count": 7,             // advertisement hit count
      "gps": 1, "lat": 39.1, "lon": -84.6,
      "first": 1718700000     // GPS epoch if a fix existed, else uptime seconds
    }
    // … up to UPLINK_BATCH_MAX (40) per POST
  ]
}
```

Notes:
- `first` is treated as a UTC timestamp only when it looks like a real epoch
  (> 1 000 000 000); otherwise it was an uptime value and is stored as "unknown".
- `devices` may be empty — BlueDriver still POSTs every interval to report
  status and to fetch commands.

## Response

```jsonc
{
  "ok": 1,
  "added": 2,               // devices newly added to the WarDriver store
  "epoch": 3914820157,      // WarDriver's current boot nonce
  "commands": [             // queued control commands for BlueDriver
    { "id": 7, "cmd": "scan", "on": 0 }
  ]
}
```

### Command grammar (what BlueDriver executes)

| `cmd`    | fields                                             | effect |
|----------|----------------------------------------------------|--------|
| `scan`   | `on:1` / `on:0` / *omit `on`*                      | start / stop / toggle scanning |
| `clear`  | —                                                  | wipe BlueDriver's device store |
| `config` | `activeScan:1` / `0`                               | set + persist active-scan mode |
| `beacon` | `type,name,arg1,arg2` (start) **or** `action:"stop"` | start / stop the beacon transmitter |
| `gatt`   | `mac:"aa:bb:.."`, `addrType:0|1`                   | connect + enumerate GATT (~8 s); result returns in the next request's `gattResult` |

Every command carries a monotonic `id`. BlueDriver runs each id once and reports
the highest it has run as `lastCmdId` in subsequent requests.

### Reliable delivery across reboots (epoch + ack)

- WarDriver picks a random non-zero `epoch` at boot and stamps it on every
  response. It assigns ascending command `id`s.
- BlueDriver mirrors the `epoch` it last saw into `linkEpoch`. If it sees a
  *new* epoch in a response, it resets its `lastCmdId` to 0 (the command-id
  sequence restarted because WarDriver rebooted).
- WarDriver prunes acknowledged commands (`id <= lastCmdId`) **only when the
  posted `linkEpoch` equals its current `epoch`** — so a stale ack sent in the
  first request after a WarDriver reboot can't wrongly clear fresh commands.

This makes command delivery exactly-once in practice, surviving a reboot of
either board.
