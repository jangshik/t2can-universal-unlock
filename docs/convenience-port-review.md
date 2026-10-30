# Convenience integration — 261029_convenience

Based on pre-release `4298151336f7cfbe7882704c2f9f96279ced24ec`.
The original firmware source was not supplied; the selected functions were
implemented from the user specification and checked against the supplied
Bayberry VEH/CH/PARTY DBCs. Continuous AP - flipper has been removed. The
existing Continuous AP implementation, configuration and UI remain unchanged.

## Controls

Settings → **Convenience & Macros** contains persistent switches and gesture
settings. All new convenience functions, including Easy Entry, default OFF.
The CAN Transmission switch uses the existing persistent listen-only mode.
S3XY BLE and steering macros share the feature action registry. Existing BLE
IDs 0–13 are unchanged; new IDs 14–54 are append-only. Actions check support at
execution time and use a bounded queue with one-second expiry.

| Feature | Juniper Body + Chassis | Juniper Party + Chassis |
| --- | --- | --- |
| CAN Transmission master | Both controllers | Both controllers |
| Kickdown | Body stock 0x118 / 0x39D / 0x334 | Chassis stock 0x118 / 0x39D / 0x334 |
| Easy Entry / mirror sync | User-controlled, default OFF | Unavailable |
| Rear left / center / right belt overrides | Separate switches, default OFF | Unavailable |
| Reverse hazard / rear fog | Body | Unavailable |
| Steering macros | Body 0x3C2 MUX1 | Unavailable in supplied DBC |
| S3XY actions | Per-action profile checks | Per-action profile checks |

Kickdown uses configurable pedal engage/release percentages and pedal, brake,
or either release policy. Braking always inhibits injection. The stock counter
is preserved and the existing additive checksum is recomputed. Manual Pedal
Response and AP drive-profile ownership take precedence.

Three macro slots support double, triple and long presses with adjustable
debounce, click gap and hold length. Nine buttons are selectable. Camera uses the user-confirmed encoding
0=inactive, 1=soft_press, 2=hard_press, 3=fault. Camera, high beam and wiper
normalize inactive/soft/hard states separately from ordinary switches. Native
button behavior remains active. Recovery, stale inputs and local TX echoes
cannot create gestures.

## Easy Entry and LAB

Enabling **Easy Entry Window** permits automatic transmission. In Park, a fresh
closed-window status, assumed closed door, dry-weather observation and a new
handle pull request 100% (raw20). After a successfully sent lowering request,
the assumed open→closed door transition requests closing. **Mirror Sync** can
request PRESENT on front-door entry. Original frame fields are preserved.
The assumed door codes remain 1=closed / 4=open and require vehicle validation.
There is no guessed STOP=6 transmission: releasing the handle does not stop
window movement. Use the vehicle's window switch to interrupt movement.

Enable LAB in Settings, then open **LAB → Easy Entry Test**. With automatic
Easy Entry OFF, select one window and percentage / vent / close / open fully,
then press **Send once**. Percentage is restricted to 0–100 in 5% steps.
The panel shows raw door/window values and their ages, the successful send
count, and the exact last eight transmitted bytes. Percentage direction and
physical responses remain unverified.

Manual tests require Juniper Body, LAB enabled, CAN transmission enabled,
fresh Park gear (≤500 ms), and a fresh idle 0x294 template (≤250 ms).
Final transport admission rechecks profile, generation, epoch, mode and gear,
and recomposes from the newest template under the TX barrier. Commands are
not persisted, queued or automatically retried. HTTP GET only reads status.
Actual transmission acceptance does not establish that the vehicle acted.

## Verification

See [DBC findings](juniper-dbc-review.md) and the reproducible
[signal contract](juniper-dbc-contract.json). Native tests cover bus isolation,
thresholds, field/checksum preservation, macro timing, NVS failures, strict API
parsing, stale/epoch/final-admission races and manual/automatic window controls.
Browser tests exercise source and generated dashboard, including single-shot
LAB sending and failure recovery. The generated dashboard is 94,618 gzip bytes.

Firmware target: ESP32-S3, ESP32 Arduino core 3.3.11, autowp-mcp2515 1.3.1,
16 MB flash, OPI PSRAM, `app3M_fat9M_16MB`, hardware CDC, loop/events on core 1.
See `tools/build_convenience_firmware.sh` for the reproducible compile command.
Validation completed: **10 native C++ binaries, 15 Python test entrypoints,
and 3 browser suites passed**. Browser suites check both source and embedded
artifacts. Full ESP32-S3 compile/link passed: program 1,598,735 / 3,145,728 bytes;
static RAM 94,008 / 327,680 bytes. The cross-compiler exposed a macro-config
brace-assignment incompatibility; explicit value initialization fixed it.
No flash or vehicle-operation test has been performed.

Camera update: all 25 host test entrypoints passed again, plus the convenience
browser suite on source and embedded artifacts and the full ESP32-S3 build.
The original v261029_convenience tag predates this update.

## Sensor automation update

See [Automations](automations.md) for the sixteen-rule editor with independent persistent enable/disable toggles, low-speed camera
ownership, sensor freshness and runtime cancellation behavior. Reverse Hazard
now requires fresh BUTTON feedback before an automatic OFF request.
