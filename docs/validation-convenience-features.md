# Convenience feature logic review and validation

Scope: the additions on `261029_convenience` through `7582a2b`, plus the fixes
from this review. Existing core affinity, task priorities, CAN driver setup and
build core options are retained. This is a source, host, browser and build
validation; no vehicle or physical ESP32 was connected.

## Findings reproduced and fixed

The original 22 selected host suites passed, but additional production-path
interleaving checks reproduced six failures, grouped into four causes:

| Cause | Reproduction | Fix |
| --- | --- | --- |
| Camera ownership checked only by the web service | OFF→ON, Reverse→Drive, or an RX gap could occur and recover between service ticks, retaining ownership of a camera the driver had intervened in | While a camera operation/ownership exists, check cancellation before and after updating each observed CAN frame |
| Missing continuity information in rule timers | A stalled evaluator or an invalid/stale input replaced by a fresh value could carry a pending delay across the unobserved interval | Rebaseline after evaluation gaps over 500 ms; keep a per-sample continuity generation for invalid/gapped input; relinquish camera ownership when its rule rebases |
| A convenience toggle erased the shared action queue | Two rules enqueue Kickdown and Reverse Fog; executing the first clears the second | Queued convenience toggles preserve pending siblings; explicit convenience configuration saves still cancel pending actions |
| Disabling/editing a rule left its queued action alive | Disable a rule after its action has been queued but before dispatch | Successful rule changes clear pending generic actions; failed or unchanged saves do not |

The shared action queue has no source-rule identity. Therefore a successful rule
edit cancels all currently queued generic actions, including BLE/macro entries,
rather than risking execution of an edited/disabled rule. It does not reset
unchanged rules' timers. Controller/mode reset boundaries still cancel pending
work, and the queue's existing capacity (16) and expiry (1 second) remain.

The new `test_automation_interleavings_host.py` runs nine independent production
regressions: camera OFF/ON, transient Reverse, camera RX gap, camera service gap,
rule service gap, sensor gap, invalid sensor recovery, sibling toggles, and
queued action cancellation on disable. The disable test also verifies that
failed and unchanged saves preserve pending actions. A camera rule rebaseline
leaves the existing camera open; it must not bypass its configured exit delay.

## Coverage

| Area | Covered behavior |
| --- | --- |
| 16 saved rules | Inclusive speed bounds, gear/brake validation, enter/exit delays, multiple masks including rule 16, enable/disable, independent state, NVS v1→v2 migration, corrupt records, replay prevention |
| Low-speed camera | Existing camera ON protection, accepted press/release, OFF→ON ownership confirmation, exit hold and re-entry, physical request/screen intervention, final-admission races, stale RX, mode/epoch changes, failed TX |
| Reverse hazards / three flashes | Reverse edges, already active hazards, manual button/alarm override, pulse duration, ownership, final-admission cancellation, failed TX, stale feedback, flash-count completion |
| Easy Entry mirrors | Front handle edge, Park/near-zero speed, both initially unfolded, fold completion, optional Drive speed/time return, speed dip, manual/foreign requests, asymmetric states, timeout, no retry, conservative reset/reboot, config migration |
| Wi-Fi visibility | Three modes, boot deadline including zero and long uptime, explicit NVS saves vs RAM-only runtime changes, no AP/MCU restart path, strict HTTP validation, failed save/get/set/storage, bounded retries, recovery AP visibility |
| Dashboard | Source and embedded firmware HTML, settings/toggles, save failure handling, input preservation, mobile layout, no Wi-Fi reboot overlay, current AP and settings regressions |
| Integration | Current Continuous AP runtime, NAG arbitration, final TX barrier, API/config persistence, shared feature queue, macros, unchanged core allocation |

## Test commands and results

C++ programs are built with `-std=c++17 -O1 -g -Wall -Wextra -Werror
-Wno-unused-function -fsanitize=undefined -fno-sanitize-recover=all`:

- `test_automation_pure.cpp`, `test_automation_api_host.cpp`
- `test_convenience_pure.cpp`, `test_convenience_runtime_host.cpp`, `test_convenience_api_host.cpp`
- `test_easy_entry_mirror.cpp`, `test_easy_entry_mirror_runtime.cpp`
- `test_feature_actions_pure.cpp`, `test_feature_actions_runtime_host.cpp`, `test_macro_gesture_pure.cpp`

Python entry points:

- `test_automation_interleavings_host.py` (also enables undefined-behavior checks)
- `test_wifi_ap_visibility_host.py`
- `test_settings_nvs_contracts.py`, `test_web_serialization_contracts.py`, `test_web_format_buffers_host.py`
- `test_dashboard_layout_contracts.py`, `test_dashboard_profiles_contracts.py`
- `test_continuous_ap_nag_host.py`, `test_continuous_ap_runtime_host.py`, `test_continuous_ap_admission_races_host.py`
- `test_continuous_ap_config_host.py`, `test_continuous_ap_api_host.py`, `test_continuous_ap_tx_host.py`

Browser suites, each against both editable source and exact embedded HTML:

- `test_dashboard_convenience.js`
- `test_dashboard_wifi_visibility.js`
- `test_dashboard_continuous_ap.js`
- `test_dashboard_settings_controls.js`

`tools/verify_juniper_dbc.py` validates 87 signal fields plus the required value
tables against all three supplied Bayberry DBCs. Generated output matches the
checked-in contract (order-independent comparison). The embedded dashboard
matches its source and remains below its 100,000-byte gzip limit.

The final host run passes all 23 selected executable/entry-point suites, including
all nine new interleavings. All four browser suites pass on both artifacts.
The final ESP32 build succeeds: 1,616,339 bytes flash (51%) and 94,720 bytes
global RAM (28%). Detailed logs are included in the validation artifact.
Firmware target: ESP32-S3, 16 MB flash, OPI PSRAM, app3M_fat9M_16MB, Arduino core
3.3.11. Board core options are unchanged.

## Known legacy-test failure

`test_v328_continuous_ap_nag_host.py` still fails its Party+Chassis expectation
that `tsl9InputSendCanB(old,command)` uses a neutral right-wheel field (0x10).
This is reproduced on the untouched `e11c216` baseline as well. Its historical
expectation does not match the current Body-only Continuous AP policy. The
current NAG integration and Continuous AP runtime/admission suites pass. The
legacy test is reported as a known failure, not counted as passing or silently
weakened.

## Limits

These results do not measure real CAN bus load, RX overflow, interrupt latency,
ESP32 watchdog behavior, ECU command acceptance, physical mirror/camera/light
movement, or Wi-Fi client continuity. No guarantee of zero frame loss follows
from host tests or compilation. Flash writes caused by explicit settings saves
and persistent toggle actions remain part of the existing runtime architecture;
this review does not repartition cores or redesign NVS/CAN scheduling.

Rule evaluation uses sampled conditions; it does not promise to emit an action
for every arbitrarily short sensor pulse between evaluator ticks. Automatic
camera cancellation now observes received transitions directly. A manual action
that produces neither an observable request nor a relevant state change cannot
be proven from CAN state alone.
