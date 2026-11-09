# Wi-Fi SSID visibility

Settings → Wireless → Wi-Fi Name Visibility offers three persisted modes:

| Mode | Behavior |
| --- | --- |
| Always visible (default) | Broadcast the dashboard AP's name |
| Always hidden | Configure the AP with a hidden SSID at startup |
| Hide after boot delay | Initially visible, then hide after 0–3600 seconds from device boot |

The saved delay defaults to 60 seconds. Zero seconds applies hiding at AP
startup. Saving a delay whose boot deadline has passed requests hiding after
the HTTP response (600 ms allowance). It does not start a new delay from the
save time. Selecting Always visible reveals the SSID again. A hidden AP stays
on; connect manually with its exact SSID and password if the phone does not
rejoin automatically. Existing credentials, channel, security and IP are kept.

## Reboot and connection behavior

The production build uses Arduino-ESP32 3.3.11. Its `WiFiAPClass::softAP()` passes
`ssid_hidden` to `APClass::create()`, which ultimately calls
`esp_wifi_set_config(WIFI_IF_AP, ...)`. The installed ESP-IDF header documents
this as a configuration API for an enabled interface. No MCU reboot is required
by this API.

The new live path reads the AP configuration, changes only `ap.ssid_hidden`,
sets it with `esp_wifi_set_config`, then reads it back. It does not call
`ESP.restart`, `esp_restart`, `WiFi.mode`, `softAPdisconnect`, `esp_wifi_stop`,
`esp_wifi_start`, or the existing AP reactivation routine. An error is reported;
it never attempts an AP or MCU restart as recovery. The driver is retried at
most three times, five seconds apart. Save Visibility can retry a failed change.

This is source/API and host-test verification, not a physical ESP32 connection
or uptime measurement. Driver behavior and phone reconnection should still be
checked on hardware. An SSID visibility change must not be presented as a
password change or as a device reboot. Actual SSID/password edits continue to
use the pre-existing AP-only reactivation behavior.

## Scheduling, storage and core allocation

The feature runs inside the existing web task on core 0. Existing tasks,
priorities, core affinity, CAN processing and build core options are unchanged.
The delay uses 64-bit monotonic uptime from `esp_timer_get_time()`; it neither
sleeps nor wraps back to visible after the 32-bit `millis()` rollover. No new
interrupt, hardware timer or FreeRTOS task is created, and the visibility path
acquires no CAN mutex or spinlock.

Explicit Save Visibility stores one versioned 8-byte blob at
`wifiap/visibility`. Unchanged saves do not rewrite NVS. Missing records default
to visible; malformed/read-error records default to visible with an error flag.
A failed save leaves the active policy unchanged. Wi-Fi driver configuration
uses RAM storage (`WiFi.persistent(false)` before initialization and
`esp_wifi_set_storage(WIFI_STORAGE_RAM)` before a live change), so reaching the
hide deadline does not write flash. Explicit saves still use NVS and are not a
general solution for the existing firmware's flash/CAN timing constraints.

Reset Feature Settings preserves the `wifiap` namespace. Factory Reset / Reset
Setup clears it and returns visibility to Always visible. If starting custom
credentials fails and the existing recovery AP is selected, that recovery AP
stays visible until a visibility save, a subsequent successful credential apply,
or the next boot. The status exposes this fallback instead of claiming the
saved hidden policy is active.

## API

`POST /api/wifi/visibility?mode=2&hideAfterSec=60` saves the policy independently
of credentials. Both arguments are required. Mode is 0 visible, 1 hidden,
2 delayed; delay is an integer 0–3600. Duplicate, unknown, malformed and
out-of-range parameters are rejected. Response success means settings saved,
not that the asynchronous visibility change is already complete.

`GET /api/wifi/status` retains existing fields and adds:

- `visibilityMode`, `hideAfterSec`: saved policy.
- `ssidHidden`, `visibilityKnown`: last verified driver state.
- `visibilityPending`, `visibilityError`: application status/error.
- `visibilityNvsError`, `visibilityFallback`: storage/recovery status.
- `hideInSec`: remaining time until the boot deadline.
- `uptimeSec`: monotonic uptime for checking continuity on hardware.

A successful set/readback confirms driver configuration, not a radio scan.
The browser preserves unsaved SSID/password and visibility edits during polling,
shows save/application failures, and does not open the reboot overlay for this
operation.

## Validation

- `python3 tests/test_wifi_ap_visibility_host.py`: real production API/runtime
  with IDF/NVS fakes; input validation, reboot loading, timer boundary and long
  uptime, unchanged AP fields, NVS/driver failures and bounded retries.
- `node tests/test_dashboard_wifi_visibility.js`: source and embedded dashboard;
  modes, delay editing, polling, independent credential controls, errors,
  reconnect recovery, mobile width and absence of a reboot overlay.
- Standard dashboard serialization/layout/NVS checks and ESP32-S3 compilation.
