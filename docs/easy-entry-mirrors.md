# Easy Entry mirrors

Easy Entry Mirror Fold folds the mirrors to make front-door access easier in a
narrow parking space. Mirror Auto Return unfolds them only after a confirmed
Easy Entry fold in the current uninterrupted session and a sustained driving
speed. Neither selecting Drive/Reverse nor closing a door unfolds the mirrors.

## Settings

In Settings → Convenience & Macros:

| Setting | Default | Range / behavior |
| --- | --- | --- |
| Easy Entry Mirror Fold | OFF | Independent of Easy Entry Window |
| Easy Entry Weather Guard | ON | Blocks new window lowering and mirror folding in rain/snow or without fresh weather data |
| Mirror Auto Return | OFF | Applies only to folds initiated while this is enabled |
| Return speed | 20 km/h | 5–50 km/h, inclusive threshold |
| Hold speed for | 2 seconds | 1–30 seconds continuously at/above threshold |

All switches and the speed/time settings persist in NVS. Changing any mirror
setting cancels the current return permission; enabling Auto Return after a fold
does not retroactively authorize unfolding. Existing v1 Mirror Sync settings
are migrated with both mirror switches OFF because the old setting unfolded on
entry and must not silently enable the new folding behavior. Other convenience
and macro settings are retained. The stable API key/action key `mirrorSync` /
`mirror_sync_toggle` is retained; action 41 is now labeled Easy Entry Mirror Fold.

## Fold and return conditions

A fresh released→pulled front-door handle edge, Park, near-zero vehicle speed
(0–0.08 km/h to cover one speed quantization step), both mirrors confirmed
UNFOLDED, and a fresh idle stock mirror-request frame are required to request
folding. The first handle sample establishes a baseline; a held handle at boot
is not a new edge. Rear handles and window position do not trigger or
control mirror folding. With Weather Guard ON, a dry weather frame received
within 2 seconds is also required; rain, snow, missing or stale weather blocks
new folding and window lowering. OFF removes only the weather condition.
Clearing weather does not replay a blocked handle pull: release and pull again.
Weather does not block closing an automatically lowered window or the normal
speed/time-based return of mirrors already folded by Easy Entry. Unknown, moving, already folded or asymmetric mirrors
cannot grant a new return permission.

A successful CAN send is not physical confirmation. Both mirrors must report
FOLDED after our accepted fold request, within 5 seconds, with continuous fresh
feedback while parked. Only then can Auto Return be armed. Partial movement,
failed transmission, timeout or leaving Park before confirmation cancels it.

For return, both mirrors must remain FOLDED, Auto Return must remain ON, gear
must be Drive, and speed must stay at/above the configured threshold for the
configured duration. Falling below the threshold or leaving Drive restarts the
hold timer. Return is sent once on a fresh idle stock request frame; final
transport admission rechecks current inputs and copies the latest stock fields.
Failed or unconfirmed unfolding is not retried. Actual UNFOLDED feedback from
both mirrors completes the operation.

## Conservative ownership and reboot

Return permission is deliberately volatile. Settings survive reboot, but an
old flash record and a matching folded position cannot prove that no manual
operation occurred while the device was off. After reboot/reset, a CAN epoch
change, listen-only transition or lost feedback, automatic return is disarmed.
Already folded mirrors remain untouched. Manually unfold them using the vehicle
controls if needed; a future newly observed Easy Entry fold can arm a new return.

Both mirror states, gear and speed must remain fresh within 500 ms. The mirror
request template must remain available within 500 ms for ownership, and within
250 ms and idle for a command/continuous return hold. Unknown/invalid feedback
or an unexpected position change cancels return. Any non-idle observed UI or
DAS mirror request cancels it, even if mirror position does not change. There is
no timing-based exemption for requests that might be our own echoes: an
indistinguishable echo also cancels return rather than hiding a possible user
request. Settings saves and sensor traffic do not block the CAN receive loop;
there are no sleep/while waits for the speed hold.

CAN position reports do not identify the origin of a fold. These rules do not
claim reliable detection of manual operations that produce no observable CAN
request or position change. The device also does not suppress the vehicle's
own folding/unfolding behavior. Vehicle-side automatic unfolding can still
occur according to its settings.

## Signals and verification

Supplied Bayberry VEH DBC contracts:

- `0x102` / `0x103`: left/right front handle at bit 10 and mirror fold state at
  bit 52, length 3. States: 0 UNKNOWN, 1 FOLDED, 2 UNFOLDED, 3 FOLDING,
  4 UNFOLDING. API values outside 0–7 are not on-wire states.
- `0x273`: UI mirror request at bit 24, length 2: 0 IDLE, 1 RETRACT,
  2 PRESENT, 3 SNA. Preserve all other fields.
- `0x3E9`: DAS mirror request at bit 25, length 2.
- `0x61D`: `UI_weatherSvcRaining` at bit 56 and `UI_weatherSvcSnowing` at bit 57.
  The supplied DBC has no separate hail signal. Hail is blocked only if the
  vehicle reports rain or snow; independent hail detection is not available.
  Fresh CAN reception does not establish the age or accuracy of the upstream
  weather service. No unverified service-age units are assumed.
- `0x118`: gear; `0x257`: vehicle speed in km/h, independent of display units.

API configuration adds `mirrorAutoReturn`, `mirrorReturnSpeedKph` and
`mirrorReturnDelaySec`, plus `easyEntryWeatherGuard` (boolean), to `/api/convenience/config`. Status includes
`mirrorLeftState`, `mirrorRightState`, and `mirrorReturnState`:
0 idle/unarmed, 1 fold queued, 2 waiting for fold confirmation, 3 armed,
4 return queued, 5 waiting for unfold confirmation. Settings use version 3 of
the existing 32-byte `features/convenience` record. Both v1 and v2 records
migrate with Weather Guard ON; v1 retains the conservative mirror migration.
The weather condition is checked again at final CAN admission for lowering and
folding, without adding waits, network requests, tasks or core-affinity changes.

Host tests cover existing folded/asymmetric mirrors, one-shot completion,
failed sends and incomplete movement, same-state external fold requests,
reboot/migration, stale inputs, speed/time boundaries and resets, millisecond
wrap, disabled controls, and input/settings changes at final TX admission.
The browser test exercises source and embedded dashboard settings, persistence
responses and failed saves. Real vehicle movement and request-source behavior
still require vehicle validation; DBC checks and compilation do not establish
physical ECU acceptance.
