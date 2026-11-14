# Easy Entry weather guard validation

The optional `easyEntryWeatherGuard` setting defaults ON and persists in the
version 3 convenience record. Existing v1/v2 records migrate with the guard ON.
The setting applies to new Easy Entry window lowering and front-handle mirror
folding. Rain/snow, absent weather, or a CAN weather reception older than 2 s
blocks those operations. OFF bypasses only this weather requirement.

Weather is checked both when creating a command and at final transport
admission. Blocked handle pulls are consumed; dry weather or disabling the
setting does not replay them. Window closing and owned mirror return retain
their existing conditions and remain available regardless of weather.

The verified VEH DBC supplies rain/snow at bits 56/57 of `0x61D`. It supplies no
separate hail indicator. Hail can only be covered when reported as rain/snow.
CAN reception age is checked; upstream weather-service accuracy or update age
is not established by these signals. No network lookup, new task, delay loop,
core reassignment or CAN-driver change is introduced.

## Regression coverage

`tests/test_easy_entry_weather.cpp` exercises the actual runtime and HTTP API:

- Both front handles with the guard ON/OFF for dry, rain, snow, both, absent,
  malformed, and expired weather; inclusive 2 s boundary and millis wrap.
- Park checks retained with the guard OFF; no replay after improving weather
  or a guard setting change while a handle stays pulled.
- Rain/snow or weather expiration between output creation and final TX admission;
  rejected mirror sends leave no automatic-return ownership.
- Closing an owned window and returning owned mirrors in wet or stale weather.
- Defaults, NVS round-trip, v1/v2 migration, corrupt v3 bytes, failed saves,
  strict HTTP booleans, duplicate keys and unsupported connection handling.

The source and embedded dashboard browser tests cover default ON, OFF/ON saves,
reload from the API, and unsupported-profile disabling.

The following regression run passes 24 host suites (11 C++ executables and 13
Python entry points): the 23 suites listed in
[the broader feature validation](validation-convenience-features.md), plus the
new weather suite. C++ builds use `-std=c++17 -O1 -g -Wall -Wextra -Werror
-Wno-unused-function -fsanitize=undefined -fno-sanitize-recover=all`.

All four existing browser suites pass for source and embedded HTML: convenience,
Wi-Fi visibility, Continuous AP, and settings controls. All 87 DBC field checks
pass. Generated dashboard gzip size is 99,355 bytes, below the 100,000-byte cap.
The previously documented legacy AP test mismatch is unchanged and is not
counted among these passing suites.

ESP32-S3 compilation succeeds with Arduino core 3.3.11, 16 MB flash, OPI PSRAM,
`app3M_fat9M_16MB`, and unchanged `LoopCore=1,EventsCore=1`. Flash use is
1,616,607 bytes (51%); global RAM is 94,720 bytes (28%). The compiled staging
headers and dashboard were compared with the final sources and match.

No physical ESP32 or vehicle test was performed. Actual weather reporting,
actuator response and CAN timing still require hardware verification.
