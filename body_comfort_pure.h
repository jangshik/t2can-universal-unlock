#pragma once

// Body Comfort features (add_feats) — pure, host-testable logic.
//
// Target: HW4 Model Y Juniper (Standard Body + Chassis, Body/VEH CAN on CAN A).
// Model Y L (Party + VH) carries the same VEH frames on CAN B and reuses this
// logic unchanged. Signal layouts follow the Bayberry VEH DBC:
//
//   0x102 VCLEFT_doorStatus      front/rear latch, left mirror fold state
//   0x103 VCRIGHT_doorStatus     front/rear latch, right mirror fold state
//   0x118 DI_systemStatus        DI_gear (21|3)
//   0x257 DI_speed               DI_vehicleSpeed (43|13, 0.08 kph, -40)
//   0x25C APP_environment        APP_environmentRainy (0), Snowy (1)
//   0x273 UI_vehicleControl      UI_rearFogSwitch (23), UI_mirrorFoldRequest (24|2)
//   0x294 UI_vehicleControl3     UI_windowPercentageRequest/Requested*/Request
//   0x2C2 VCLEFT_windowStatus    LF/LR window state, position, position state
//   0x2C3 VCRIGHT_windowStatus   RF/RR window state, position, position state
//   0x353 UI_status              UI_cameraActive (41)
//   0x3A1 VCFRONT_vehicleStatus  mux0 2Row{Left,Center,Right}Unbuckled (36/38/40|2)
//   0x3B3 UI_vehicleControl2     UI_lightSwitch (9|3)
//   0x3C2 VCLEFT_switchStatus    mux0 hazard button / window switches,
//                                mux1 steering-wheel switches + camera button
//   0x3F5 VCFRONT_lighting       hazardLightRequest (4|4), indicatorLeft (0|2)
//
// Nothing in this file touches hardware. Every byte the runtime transmits is a
// copy of a fresh stock frame with only the documented fields changed.

#include <stdint.h>
#include <string.h>
#include "tesla_can_fields_pure.h"

// ─── CAN identifiers ─────────────────────────────────────────────────────
static constexpr uint16_t BC_ID_VCLEFT_DOOR = 0x102u;
static constexpr uint16_t BC_ID_VCRIGHT_DOOR = 0x103u;
static constexpr uint16_t BC_ID_DI_SYSTEM = 0x118u;
static constexpr uint16_t BC_ID_DI_SPEED = 0x257u;
static constexpr uint16_t BC_ID_APP_ENV = 0x25Cu;
static constexpr uint16_t BC_ID_UI_VEHICLE_CONTROL = 0x273u;
static constexpr uint16_t BC_ID_UI_VEHICLE_CONTROL3 = 0x294u;
static constexpr uint16_t BC_ID_VCLEFT_WINDOW = 0x2C2u;
static constexpr uint16_t BC_ID_VCRIGHT_WINDOW = 0x2C3u;
static constexpr uint16_t BC_ID_UI_STATUS = 0x353u;
static constexpr uint16_t BC_ID_VCFRONT_VEHICLE_STATUS = 0x3A1u;
static constexpr uint16_t BC_ID_UI_VEHICLE_CONTROL2 = 0x3B3u;
static constexpr uint16_t BC_ID_VCLEFT_SWITCH = 0x3C2u;
static constexpr uint16_t BC_ID_VCFRONT_LIGHTING = 0x3F5u;
static constexpr uint16_t BC_ID_VCSEC_AUTH = 0x339u;  // VCSEC_authentication (lock state)

// ─── Timing constants ────────────────────────────────────────────────────
static constexpr uint32_t BC_GEAR_FRESH_MS = 1500u;
static constexpr uint32_t BC_SPEED_FRESH_MS = 1000u;
static constexpr uint32_t BC_DOOR_FRESH_MS = 1500u;
static constexpr uint32_t BC_WINDOW_FRESH_MS = 2000u;
static constexpr uint32_t BC_WEATHER_FRESH_MS = 5000u;
static constexpr uint32_t BC_LIGHTING_FRESH_MS = 1500u;
static constexpr uint32_t BC_UI_STATUS_FRESH_MS = 3000u;
static constexpr uint32_t BC_SWITCH_STALE_MS = 600u;
static constexpr uint32_t BC_LOCK_FRESH_MS = 3000u;
// Same press shape the stalkless turn-signal path already uses on 0x3C2.
static constexpr uint32_t BC_BUTTON_PRESS_MS = 300u;
static constexpr uint32_t BC_BUTTON_RELEASE_MS = 200u;
static constexpr uint32_t BC_HAZARD_SETTLE_MS = 1200u;
static constexpr uint8_t BC_HAZARD_MAX_ATTEMPTS = 3u;
static constexpr uint8_t BC_ONE_SHOT_FRAMES = 3u;
static constexpr uint32_t BC_ONE_SHOT_TIMEOUT_MS = 2000u;

// ─── Little-endian (Intel) bit helpers ───────────────────────────────────
static inline uint32_t bcGetBitsPure(const uint8_t *d, uint8_t start, uint8_t len) {
  if (d == nullptr || len == 0u || len > 32u || (uint16_t)start + len > 64u) return 0u;
  uint32_t v = 0u;
  for (uint8_t i = 0; i < len; ++i) {
    const uint8_t bit = (uint8_t)(start + i);
    if (d[bit >> 3] & (uint8_t)(1u << (bit & 7u))) v |= (1UL << i);
  }
  return v;
}

static inline void bcSetBitsPure(uint8_t *d, uint8_t start, uint8_t len, uint32_t value) {
  if (d == nullptr || len == 0u || len > 32u || (uint16_t)start + len > 64u) return;
  for (uint8_t i = 0; i < len; ++i) {
    const uint8_t bit = (uint8_t)(start + i);
    const uint8_t mask = (uint8_t)(1u << (bit & 7u));
    if (value & (1UL << i)) d[bit >> 3] |= mask;
    else d[bit >> 3] &= (uint8_t)~mask;
  }
}

static inline bool bcAgeFreshPure(uint32_t now, uint32_t stampMs, bool valid,
                                  uint32_t limitMs) {
  return valid && (uint32_t)(now - stampMs) <= limitMs;
}

// ─── Doors ───────────────────────────────────────────────────────────────
enum BcDoorIndexPure : uint8_t {
  BC_DOOR_FL = 0, BC_DOOR_FR = 1, BC_DOOR_RL = 2, BC_DOOR_RR = 3, BC_DOOR_COUNT = 4
};

enum BcDoorStatePure : uint8_t {
  BC_DOOR_STATE_UNKNOWN = 0,
  BC_DOOR_STATE_CLOSED = 1,
  BC_DOOR_STATE_OPEN = 2,
};

// Latch: 0 SNA 1 OPENED 2 CLOSED 3 CLOSING 4 OPENING 5 AJAR 6 TIMEOUT 7 DEFAULT 8 FAULT.
// A closing door is not latched yet, so it still counts as open.
static inline uint8_t bcDoorStateFromLatchPure(uint8_t latch) {
  switch (latch) {
    case 1u: case 3u: case 4u: case 5u: return BC_DOOR_STATE_OPEN;
    case 2u: return BC_DOOR_STATE_CLOSED;
    default: return BC_DOOR_STATE_UNKNOWN;
  }
}

struct BcDoorFramePure {
  uint8_t frontState;
  uint8_t rearState;
  uint8_t mirrorFold;  // 0 UNKNOWN 1 FOLDED 2 UNFOLDED 3 FOLDING 4 UNFOLDING
  bool frontHandlePulled;  // VCLEFT/VCRIGHT_frontHandlePulled (10|1), exterior handle
};

// Identical bit positions on 0x102 (left side) and 0x103 (right side).
static inline bool bcDecodeDoorFramePure(const uint8_t *d, uint8_t dlc,
                                         BcDoorFramePure &out) {
  out = {};
  if (d == nullptr || dlc < 8u) return false;
  out.frontState = bcDoorStateFromLatchPure((uint8_t)bcGetBitsPure(d, 14, 4));
  out.rearState = bcDoorStateFromLatchPure((uint8_t)bcGetBitsPure(d, 18, 4));
  out.mirrorFold = (uint8_t)bcGetBitsPure(d, 52, 3);
  out.frontHandlePulled = bcGetBitsPure(d, 10, 1) != 0u;
  return true;
}

// ─── Windows ─────────────────────────────────────────────────────────────
enum BcWindowMotionPure : uint8_t {
  BC_WINDOW_STOPPED = 0, BC_WINDOW_SHORT_DROP = 4, BC_WINDOW_SHORT_DROP_REVERSE = 5
};

enum BcWindowPosStatePure : uint8_t {
  BC_WINPOS_UNKNOWN = 0, BC_WINPOS_CLOSED = 1, BC_WINPOS_CRACKED = 2,
  BC_WINPOS_VENT = 3, BC_WINPOS_PARTIAL = 4, BC_WINPOS_FULL = 5,
  BC_WINPOS_CLOSED_TRIM_CLEAR = 6,
};

struct BcWindowFramePure {
  uint8_t mux;          // 0: motion/position, 1: position state
  uint8_t motionA, motionB;
  int16_t posRawA, posRawB;  // signed 9-bit raw (1.25 per step, +265 offset)
  uint8_t posStateA, posStateB;
  uint8_t unavailA, unavailB;  // mux1 windowCtrlUnavailable: 0 NONE 1 UNEXPECTED 2 SWITCH_PRESSED
                               // 3 PINCH 4 THERMAL_LIMIT 5 UNCALIBRATED 6 VEHICLE_IN_DRIVE 7 WINDOW_MOVING
  uint8_t stopA, stopB;        // mux1 windowStopReason: 0 NONE 1 DOOR_OPEN_TRIM_PROTECT 2 NEW_REQUEST
                               // 3 TARGET_REACHED 4 STOP_REQUEST 5 FULL_OPEN_SOFT_STOP 6 STATE_TIMEOUT
};

static inline int16_t bcSigned9Pure(uint32_t raw) {
  raw &= 0x1FFu;
  return (int16_t)((raw & 0x100u) ? (int32_t)raw - 512 : (int32_t)raw);
}

// 0x2C2 A=LF B=LR ; 0x2C3 A=RF B=RR. Bit positions are identical.
static inline bool bcDecodeWindowFramePure(const uint8_t *d, uint8_t dlc,
                                           BcWindowFramePure &out) {
  out = {};
  if (d == nullptr || dlc < 8u) return false;
  out.mux = (uint8_t)bcGetBitsPure(d, 0, 1);
  if (out.mux == 0u) {
    out.motionA = (uint8_t)bcGetBitsPure(d, 1, 4);
    out.posRawA = bcSigned9Pure(bcGetBitsPure(d, 22, 9));
    out.motionB = (uint8_t)bcGetBitsPure(d, 33, 4);
    out.posRawB = bcSigned9Pure(bcGetBitsPure(d, 54, 9));
  } else {
    out.posStateA = (uint8_t)bcGetBitsPure(d, 49, 3);
    out.posStateB = (uint8_t)bcGetBitsPure(d, 52, 3);
    out.unavailA = (uint8_t)bcGetBitsPure(d, 38, 3);
    out.unavailB = (uint8_t)bcGetBitsPure(d, 45, 3);
    out.stopA = (uint8_t)bcGetBitsPure(d, 11, 3);
    out.stopB = (uint8_t)bcGetBitsPure(d, 28, 3);
  }
  return true;
}

// Per-window learned travel references. Learned only while the window is
// stopped at the stock CLOSED / FULL_OPEN classification, so a later partial
// position can be expressed as a percentage without trusting the DBC offset.
struct BcWindowLearnPure {
  bool closedValid;
  int16_t closedRaw;
  bool openValid;
  int16_t openRaw;
};

static inline void bcWindowLearnPure(BcWindowLearnPure &learn, uint8_t motion,
                                     uint8_t posState, int16_t posRaw) {
  if (motion != BC_WINDOW_STOPPED) return;
  if (posState == BC_WINPOS_CLOSED) { learn.closedValid = true; learn.closedRaw = posRaw; }
  else if (posState == BC_WINPOS_FULL) { learn.openValid = true; learn.openRaw = posRaw; }
}

static constexpr int16_t BC_WINDOW_MIN_TRAVEL_RAW = 20;

// Percent OPEN, rounded to the 5 % resolution of UI_windowPercentageRequest.
static inline bool bcWindowPercentOpenPure(const BcWindowLearnPure &learn,
                                           int16_t posRaw, uint8_t &pctOut) {
  pctOut = 0u;
  if (!learn.closedValid || !learn.openValid) return false;
  const int32_t span = (int32_t)learn.openRaw - (int32_t)learn.closedRaw;
  if (span > -BC_WINDOW_MIN_TRAVEL_RAW && span < BC_WINDOW_MIN_TRAVEL_RAW) return false;
  int32_t pct = (((int32_t)posRaw - (int32_t)learn.closedRaw) * 1000) / span;  // 0.1 %
  if (pct < 0) pct = 0;
  if (pct > 1000) pct = 1000;
  pct = ((pct + 25) / 50) * 5;  // nearest 5 %
  pctOut = (uint8_t)pct;
  return true;
}

// UI_windowRequest: 0 IDLE 1 GOTO_PERCENT 2 GOTO_VENT 3 GOTO_CLOSED 4 GOTO_OPEN.
enum BcWindowRequestPure : uint8_t {
  BC_WINREQ_IDLE = 0, BC_WINREQ_PERCENT = 1, BC_WINREQ_VENT = 2,
  BC_WINREQ_CLOSED = 3, BC_WINREQ_OPEN = 4,
};

struct BcRestorePlanPure {
  bool valid;      // snapshot is usable for Easy Entry
  bool needed;     // a restore command must follow the door closing
  uint8_t request; // BcWindowRequestPure
  uint8_t percent; // only for BC_WINREQ_PERCENT
};

// Restore target from the settled pre-entry window classification.
static inline BcRestorePlanPure bcRestorePlanPure(uint8_t posState, bool pctValid,
                                                  uint8_t pct) {
  BcRestorePlanPure plan = {};
  switch (posState) {
    case BC_WINPOS_CLOSED:
    case BC_WINPOS_CLOSED_TRIM_CLEAR:
      plan = {true, true, BC_WINREQ_CLOSED, 0u};
      break;
    case BC_WINPOS_VENT:
      plan = {true, true, BC_WINREQ_VENT, 0u};
      break;
    case BC_WINPOS_CRACKED:
    case BC_WINPOS_PARTIAL:
      if (pctValid && pct < 5u) plan = {true, true, BC_WINREQ_CLOSED, 0u};
      else if (pctValid && pct >= 95u) plan = {true, false, BC_WINREQ_IDLE, 0u};
      else if (pctValid) plan = {true, true, BC_WINREQ_PERCENT, pct};
      // Unknown travel: return to the vent position rather than leaving the
      // window fully open.
      else plan = {true, true, BC_WINREQ_VENT, 0u};
      break;
    case BC_WINPOS_FULL:
      plan = {true, false, BC_WINREQ_IDLE, 0u};  // already fully down
      break;
    default:
      break;  // UNKNOWN: Easy Entry does not act
  }
  return plan;
}

// 0x294 window bits: percent 42|6 signed ×5, requested FL48 FR49 RL50 RR51,
// request 52|3. Only window fields change; all other bytes stay stock.
static inline uint8_t bcWindowRequestedBitPure(uint8_t door) {
  switch (door) {
    case BC_DOOR_FL: return 48u;
    case BC_DOOR_FR: return 49u;
    case BC_DOOR_RL: return 50u;
    case BC_DOOR_RR: return 51u;
    default: return 0u;
  }
}

static inline bool bcComposeWindowRequestPure(uint8_t *d, uint8_t doorMask,
                                              uint8_t request, uint8_t pct) {
  if (d == nullptr || (doorMask & 0x0Fu) == 0u || request > BC_WINREQ_OPEN) return false;
  if (pct > 100u) pct = 100u;
  // GOTO_OPEN also carries 100 % in case the VC reads the percentage field.
  const uint8_t field = request == BC_WINREQ_PERCENT ? (uint8_t)(pct / 5u)
                      : request == BC_WINREQ_OPEN ? 20u : 0u;
  bcSetBitsPure(d, 42, 6, field);
  for (uint8_t door = 0; door < BC_DOOR_COUNT; ++door)
    bcSetBitsPure(d, bcWindowRequestedBitPure(door), 1, (doorMask >> door) & 1u);
  bcSetBitsPure(d, 52, 3, request);
  return true;
}

// When the UI does not send 0x294 continuously, a window command is built from
// the newest stock 0x294 seen. One-shot request fields from that old frame are
// cleared so they are never replayed: light flash (0|2), key pair response
// (6|2), headlight pulse (12), short horn (17), fascia camera wash (26), rear
// seat adjustment (28|3, 32|3), close trunk (55), rear-display seat requests
// (61, 63).
static inline void bcClearVc3TransientPure(uint8_t *d) {
  if (d == nullptr) return;
  bcSetBitsPure(d, 0, 2, 0u);
  bcSetBitsPure(d, 6, 2, 0u);
  bcSetBitsPure(d, 12, 1, 0u);
  bcSetBitsPure(d, 17, 1, 0u);
  bcSetBitsPure(d, 26, 1, 0u);
  bcSetBitsPure(d, 28, 3, 0u);
  bcSetBitsPure(d, 32, 3, 0u);
  bcSetBitsPure(d, 55, 1, 0u);
  bcSetBitsPure(d, 61, 1, 0u);
  bcSetBitsPure(d, 63, 1, 0u);
}

static constexpr uint8_t BC_WINDOW_CMD_FRAMES = 5u;
static constexpr uint32_t BC_VC3_SYNTH_AFTER_MS = 250u;
static constexpr uint32_t BC_VC3_SYNTH_INTERVAL_MS = 100u;
static constexpr uint32_t BC_VC3_TEMPLATE_MAX_AGE_MS = 60000u;

// ─── Gear / speed / weather / UI / lighting ─────────────────────────────
enum BcGearPure : uint8_t { BC_GEAR_INVALID = 0, BC_GEAR_P = 1, BC_GEAR_R = 2, BC_GEAR_N = 3, BC_GEAR_D = 4 };

// 0x339 VCSEC_authentication: simpleLockStatus (54|2: 0 SNA 1 UNLOCKED 2 LOCKED),
// leftFrontLockStatus (19) / rightFrontLockStatus (21): 0 UNLOCKED 1 LOCKED.
struct BcLockFramePure {
  uint8_t simple;
  bool frontLocked[2];  // 0 driver (left), 1 passenger (right)
};

static inline bool bcDecodeLockPure(const uint8_t *d, uint8_t dlc, BcLockFramePure &out) {
  out = {};
  if (d == nullptr || dlc < 7u) return false;
  out.simple = (uint8_t)bcGetBitsPure(d, 54, 2);
  out.frontLocked[0] = bcGetBitsPure(d, 19, 1) != 0u;
  out.frontLocked[1] = bcGetBitsPure(d, 21, 1) != 0u;
  return out.simple == 1u || out.simple == 2u;
}

// Door-handle macro gate: the car and that front door are unlocked and the door
// is open. A pull on a locked door counts only once it has unlocked and opened.
static inline bool bcHandleMacroAllowedPure(bool lockFresh, const BcLockFramePure &lock,
                                            uint8_t side, uint8_t frontDoorState) {
  return lockFresh && side < 2u && lock.simple == 1u && !lock.frontLocked[side] &&
         frontDoorState == BC_DOOR_STATE_OPEN;
}

// DI_accelPedalPos (32|8, 0.4 %, 255 = SNA) in 0.1 %, DI_brakePedalState (19|2:
// 0 OFF, 1 ON, 2 INVALID). Both live in DI_systemStatus 0x118 next to DI_gear.
static inline bool bcDecodeAccelPedalPure(const uint8_t *d, uint8_t dlc, uint16_t &pct10) {
  pct10 = 0;
  if (d == nullptr || dlc < 5u) return false;
  const uint32_t raw = bcGetBitsPure(d, 32, 8);
  if (raw == 255u) return false;
  pct10 = (uint16_t)(raw * 4u);
  return true;
}

static inline bool bcDecodeBrakePure(const uint8_t *d, uint8_t dlc, bool &pressed) {
  pressed = false;
  if (d == nullptr || dlc < 3u) return false;
  const uint32_t raw = bcGetBitsPure(d, 19, 2);
  if (raw > 1u) return false;
  pressed = raw == 1u;
  return true;
}

static inline uint8_t bcDecodeGearPure(const uint8_t *d, uint8_t dlc) {
  if (d == nullptr || dlc < 3u) return BC_GEAR_INVALID;
  const uint8_t g = (uint8_t)bcGetBitsPure(d, 21, 3);
  return (g >= BC_GEAR_P && g <= BC_GEAR_D) ? g : (uint8_t)BC_GEAR_INVALID;
}

// Returns false for SNA. Output in 0.1 kph (may be negative when reversing).
static inline bool bcDecodeSpeedPure(const uint8_t *d, uint8_t dlc, int32_t &kph10) {
  kph10 = 0;
  if (d == nullptr || dlc < 8u) return false;
  const uint32_t raw = bcGetBitsPure(d, 43, 13);
  if (raw == 8191u) return false;
  kph10 = (int32_t)((raw * 4u) / 5u) - 400;  // raw*0.08 kph - 40 kph, in 0.1 kph
  return true;
}

static inline bool bcDecodeWeatherPure(const uint8_t *d, uint8_t dlc, bool &rainy, bool &snowy) {
  rainy = snowy = false;
  if (d == nullptr || dlc < 1u) return false;
  rainy = (d[0] & 0x01u) != 0u;
  snowy = (d[0] & 0x02u) != 0u;
  return true;
}

static inline bool bcDecodeCameraActivePure(const uint8_t *d, uint8_t dlc, bool &active) {
  active = false;
  if (d == nullptr || dlc < 6u) return false;
  active = bcGetBitsPure(d, 41, 1) != 0u;
  return true;
}

// hazardLightRequest: 0 NONE 1 BUTTON 2 LOCK 3 UNLOCK 4 MISLOCK 5 CRASH 6 ALARM 7 DAS 8 UDS
static constexpr uint8_t BC_HAZARD_REQ_NONE = 0u;
static constexpr uint8_t BC_HAZARD_REQ_BUTTON = 1u;

static inline bool bcDecodeLightingPure(const uint8_t *d, uint8_t dlc,
                                        uint8_t &hazardReq, bool &indicatorLeftOn) {
  hazardReq = 0u; indicatorLeftOn = false;
  if (d == nullptr || dlc < 1u) return false;
  indicatorLeftOn = bcGetBitsPure(d, 0, 2) != 0u;
  hazardReq = (uint8_t)bcGetBitsPure(d, 4, 4);
  return true;
}

// ─── 0x3C2 VCLEFT_switchStatus ───────────────────────────────────────────
static inline uint8_t bcSwitchMuxPure(const uint8_t *d) {
  return d ? (uint8_t)bcGetBitsPure(d, 0, 1) : 0xFFu;
}

static constexpr uint8_t BC_SWC_ON = 2u;  // SNA 0 / OFF 1 / ON 2 / FAULT 3

enum BcButtonPure : uint8_t {
  BC_BTN_LEFT_PRESS = 0,
  BC_BTN_RIGHT_PRESS,
  BC_BTN_LEFT_TILT_LEFT,
  BC_BTN_LEFT_TILT_RIGHT,
  BC_BTN_RIGHT_TILT_LEFT,
  BC_BTN_RIGHT_TILT_RIGHT,
  BC_BTN_VOICE,    // VCLEFT_swcVoiceControlPress (48|2)
  BC_BTN_CAMERA,   // VCLEFT_swcCameraButtonState (50|2)
  BC_BTN_COUNT
};
static constexpr uint8_t BC_BTN_COUNT_V1 = 6u;  // blob v1 had the six wheel controls only

static inline uint8_t bcButtonStartBitPure(uint8_t button) {
  static const uint8_t bits[BC_BTN_COUNT] = {5u, 12u, 14u, 3u, 8u, 10u, 48u, 50u};
  return button < BC_BTN_COUNT ? bits[button] : 0u;
}

static inline bool bcButtonPressedPure(const uint8_t *mux1, uint8_t button) {
  return mux1 != nullptr && button < BC_BTN_COUNT &&
         bcGetBitsPure(mux1, bcButtonStartBitPure(button), 2) == BC_SWC_ON;
}

static inline bool bcHazardButtonPressedPure(const uint8_t *mux0) {
  return mux0 != nullptr && bcGetBitsPure(mux0, 3, 1) != 0u;
}

static inline void bcSetHazardButtonPure(uint8_t *mux0, bool pressed) {
  bcSetBitsPure(mux0, 3, 1, pressed ? 1u : 0u);
}

static inline bool bcCameraButtonPressedPure(const uint8_t *mux1) {
  return mux1 != nullptr && bcGetBitsPure(mux1, 50, 2) == BC_SWC_ON;
}

static inline void bcSetCameraButtonPure(uint8_t *mux1, bool pressed) {
  bcSetBitsPure(mux1, 50, 2, pressed ? BC_SWC_ON : 1u);
}

// Driver switch pack (mux0 bits 32..47): LF 32..35, LR 36..39, RF 40..43, RR 44..47.
static inline bool bcWindowSwitchActivePure(const uint8_t *mux0, uint8_t door) {
  if (mux0 == nullptr) return false;
  static const uint8_t start[BC_DOOR_COUNT] = {32u, 40u, 36u, 44u};  // FL FR RL RR
  return door < BC_DOOR_COUNT && bcGetBitsPure(mux0, start[door], 4) != 0u;
}

// ─── 0x273 / 0x3B3 UI request fields ─────────────────────────────────────
static constexpr uint8_t BC_MIRROR_REQ_IDLE = 0u;
static constexpr uint8_t BC_MIRROR_REQ_RETRACT = 1u;
static constexpr uint8_t BC_MIRROR_REQ_PRESENT = 2u;
static constexpr uint8_t BC_MIRROR_FOLDED = 1u;
static constexpr uint8_t BC_MIRROR_UNFOLDED = 2u;
static constexpr uint8_t BC_MIRROR_FOLDING = 3u;

static inline void bcSetMirrorFoldRequestPure(uint8_t *d, uint8_t request) {
  bcSetBitsPure(d, 24, 2, request & 0x03u);
}

// UI_frunkRequest (5|1): 1 = MOVE (open). One-shot, like the touchscreen button.
static inline void bcSetFrunkRequestPure(uint8_t *d) {
  bcSetBitsPure(d, 5, 1, 1u);
}

static inline void bcSetRearFogPure(uint8_t *d, bool on) {
  bcSetBitsPure(d, 23, 1, on ? 1u : 0u);
}

static constexpr uint8_t BC_LIGHT_SWITCH_ON = 1u;  // 0 AUTO 1 ON 2 PARKING 3 OFF
static inline void bcSetLightSwitchPure(uint8_t *d, uint8_t mode) {
  bcSetBitsPure(d, 9, 3, mode & 0x07u);
}

// ─── 0x3A1 rear seat-belt chime ──────────────────────────────────────────
// mux0 VCFRONT_2Row{Left,Center,Right}Unbuckled: 0 NONE 1 OCCUPIED_AND_UNBUCKLED 2 SNA.
// Counter (52|4) is preserved; checksum byte 7 is the standard Tesla additive sum.
enum BcSeatbeltTransformResultPure : uint8_t {
  BC_SEATBELT_NOT_MUX0 = 0,
  BC_SEATBELT_BAD_CHECKSUM,
  BC_SEATBELT_UNCHANGED,
  BC_SEATBELT_CHANGED,
};

static inline uint8_t bcSeatbeltTransformPure(const uint8_t *in, uint8_t dlc,
                                              uint8_t seatMask, uint8_t out[8]) {
  if (in == nullptr || out == nullptr || dlc < 8u) return BC_SEATBELT_NOT_MUX0;
  if (bcGetBitsPure(in, 0, 1) != 0u) return BC_SEATBELT_NOT_MUX0;
  if (teslaAdditiveChecksumPure(BC_ID_VCFRONT_VEHICLE_STATUS, in) != in[7])
    return BC_SEATBELT_BAD_CHECKSUM;
  memcpy(out, in, 8);
  bool changed = false;
  for (uint8_t seat = 0; seat < 3u; ++seat) {
    if (((seatMask >> seat) & 1u) == 0u) continue;
    const uint8_t start = (uint8_t)(36u + seat * 2u);
    if (bcGetBitsPure(out, start, 2) == 1u) {
      bcSetBitsPure(out, start, 2, 0u);
      changed = true;
    }
  }
  if (!changed) return BC_SEATBELT_UNCHANGED;
  out[7] = teslaAdditiveChecksumPure(BC_ID_VCFRONT_VEHICLE_STATUS, out);
  return BC_SEATBELT_CHANGED;
}

// ─── Steering-wheel gesture detector ─────────────────────────────────────
enum BcGestureEventPure : uint8_t {
  BC_GESTURE_NONE = 0xFFu,
  BC_GESTURE_DOUBLE = 0,
  BC_GESTURE_TRIPLE = 1,
  BC_GESTURE_LONG = 2,
  BC_GESTURE_COUNT = 3,
};

struct BcGesturePure {
  bool valid;
  bool pressed;
  bool longFired;
  uint8_t clicks;
  uint32_t pressMs;
  uint32_t releaseMs;
  uint32_t lastObsMs;
};

static inline uint8_t bcGestureObservePure(BcGesturePure &g, bool pressedNow,
                                           uint32_t now, uint16_t longMs) {
  if (!g.valid) {
    g = {};
    g.valid = true;
    g.pressed = pressedNow;
    g.pressMs = now;
    g.longFired = pressedNow;  // already held when first seen: not a gesture
    g.lastObsMs = now;
    return BC_GESTURE_NONE;
  }
  g.lastObsMs = now;
  if (pressedNow && !g.pressed) {
    g.pressed = true;
    g.pressMs = now;
    g.longFired = false;
    return BC_GESTURE_NONE;
  }
  if (!pressedNow && g.pressed) {
    g.pressed = false;
    if (!g.longFired) {
      if (g.clicks < 255u) g.clicks++;
      g.releaseMs = now;
    }
    return BC_GESTURE_NONE;
  }
  if (pressedNow && !g.longFired && (uint32_t)(now - g.pressMs) >= longMs) {
    g.longFired = true;
    g.clicks = 0u;
    return BC_GESTURE_LONG;
  }
  return BC_GESTURE_NONE;
}

static inline uint8_t bcGestureTickPure(BcGesturePure &g, uint32_t now,
                                        uint16_t gapMs, uint32_t staleMs) {
  if (!g.valid) return BC_GESTURE_NONE;
  if ((uint32_t)(now - g.lastObsMs) > staleMs) {
    g = {};
    return BC_GESTURE_NONE;
  }
  if (!g.pressed && g.clicks > 0u && (uint32_t)(now - g.releaseMs) > gapMs) {
    const uint8_t clicks = g.clicks;
    g.clicks = 0u;
    if (clicks == 2u) return BC_GESTURE_DOUBLE;
    if (clicks == 3u) return BC_GESTURE_TRIPLE;
  }
  return BC_GESTURE_NONE;
}

// ─── Hazard ownership controller ─────────────────────────────────────────
// Hazards are a toggle on the physical button, so the controller presses only
// when the observed state disagrees with the request, verifies the result on
// VCFRONT_lighting, never turns off hazards it did not turn on, and backs off
// if the driver cancels them while a request is still active.
struct BcHazardCtlPure {
  bool ownedOn;
  bool pressPending;
  bool pressIntentOn;
  bool userOverride;
  bool lastDesired;
  uint8_t attempts;
  uint32_t pressMs;
};

enum BcHazardActionPure : uint8_t { BC_HAZARD_ACTION_NONE = 0, BC_HAZARD_ACTION_PRESS = 1 };

static inline uint8_t bcHazardStepPure(BcHazardCtlPure &c, uint32_t now, bool obsValid,
                                       uint8_t obsReq, bool desired) {
  if (desired != c.lastDesired) {
    c.lastDesired = desired;
    c.attempts = 0u;
    if (!desired) c.userOverride = false;
  }
  if (c.pressPending) {
    if ((uint32_t)(now - c.pressMs) < BC_HAZARD_SETTLE_MS) return BC_HAZARD_ACTION_NONE;
    c.pressPending = false;
    const bool onNow = obsValid && obsReq == BC_HAZARD_REQ_BUTTON;
    const bool offNow = obsValid && obsReq == BC_HAZARD_REQ_NONE;
    if (c.pressIntentOn && onNow) { c.ownedOn = true; c.attempts = 0u; }
    else if (!c.pressIntentOn && offNow) { c.ownedOn = false; c.attempts = 0u; }
  }
  if (!obsValid) return BC_HAZARD_ACTION_NONE;
  const bool on = obsReq == BC_HAZARD_REQ_BUTTON;
  const bool off = obsReq == BC_HAZARD_REQ_NONE;
  if (c.ownedOn && !on) {
    // Cancelled without our press (driver button, lock flash takeover, ...).
    c.ownedOn = false;
    if (desired) c.userOverride = true;
  }
  if (desired) {
    if (c.userOverride || c.ownedOn || !off) return BC_HAZARD_ACTION_NONE;
    if (c.attempts >= BC_HAZARD_MAX_ATTEMPTS) return BC_HAZARD_ACTION_NONE;
    c.attempts++;
    c.pressPending = true;
    c.pressIntentOn = true;
    c.pressMs = now;
    return BC_HAZARD_ACTION_PRESS;
  }
  if (c.ownedOn && on) {
    if (c.attempts >= BC_HAZARD_MAX_ATTEMPTS) return BC_HAZARD_ACTION_NONE;
    c.attempts++;
    c.pressPending = true;
    c.pressIntentOn = false;
    c.pressMs = now;
    return BC_HAZARD_ACTION_PRESS;
  }
  return BC_HAZARD_ACTION_NONE;
}

// ─── Door-open hazard episode (D/R) ──────────────────────────────────────
struct BcDoorHazardPure {
  bool active;
  bool closedPending;
  uint32_t closedMs;
};

static inline bool bcDoorHazardStepPure(BcDoorHazardPure &s, uint32_t now, bool enabled,
                                        bool gearDriveOrReverse, bool doorsValid,
                                        bool anyDoorOpen, uint32_t offDelayMs) {
  if (!enabled) { s = {}; return false; }
  if (!s.active) {
    if (gearDriveOrReverse && doorsValid && anyDoorOpen) {
      s.active = true;
      s.closedPending = false;
    }
    return s.active;
  }
  if (doorsValid && anyDoorOpen) {
    s.closedPending = false;
    return true;
  }
  // All closed, or door data lost: finish after the configured delay.
  if (!s.closedPending) { s.closedPending = true; s.closedMs = now; }
  if ((uint32_t)(now - s.closedMs) >= offDelayMs) { s = {}; return false; }
  return true;
}

// ─── Mirror unfold qualification ─────────────────────────────────────────
struct BcUnfoldTimerPure {
  bool qualifying;
  uint32_t sinceMs;
};

// D, fresh speed strictly above the threshold, continuously for holdS seconds.
static inline bool bcUnfoldDuePure(BcUnfoldTimerPure &t, uint32_t now, bool gearD,
                                   bool speedValid, int32_t kph10, uint8_t thresholdKph,
                                   uint8_t holdS) {
  const bool cond = gearD && speedValid && kph10 > (int32_t)thresholdKph * 10;
  if (!cond) { t = {}; return false; }
  if (!t.qualifying) { t.qualifying = true; t.sinceMs = now; }
  return (uint32_t)(now - t.sinceMs) >= (uint32_t)holdS * 1000u;
}

// ─── Configuration ───────────────────────────────────────────────────────
enum BcActionPure : uint8_t {
  BC_ACTION_NONE = 0,
  BC_ACTION_TOGGLE_EASY_ENTRY,
  BC_ACTION_TOGGLE_MIRROR_FOLD_OPT,
  BC_ACTION_TOGGLE_SEATBELT,
  BC_ACTION_RETIRED_4,  // was "toggle hazard blink availability"; loads as NONE
  BC_ACTION_TOGGLE_REVERSE_HAZARD,
  BC_ACTION_TOGGLE_REVERSE_LIGHT,
  BC_ACTION_TOGGLE_DOOR_HAZARD,
  BC_ACTION_RUN_HAZARD_BLINK,
  BC_ACTION_MIRROR_FOLD_NOW,
  BC_ACTION_CAMERA,
  // Existing (pre-add_feats) features, toggled through their own setters.
  BC_ACTION_TOGGLE_NAG_KILLER,
  BC_ACTION_TOGGLE_ISA_SUPPRESSION,
  BC_ACTION_TOGGLE_DRIVER_MONITORING,
  BC_ACTION_TOGGLE_AUTO_BLINKER,
  BC_ACTION_TOGGLE_TLSSC,
  BC_ACTION_TOGGLE_DOOR_CANCEL,
  BC_ACTION_TOGGLE_VISION_CONTROL,
  BC_ACTION_TOGGLE_CONTINUOUS_AP,
  BC_ACTION_TOGGLE_BATTERY_PREHEAT,
  BC_ACTION_OPEN_FRUNK,           // 20: P only
  BC_ACTION_TOGGLE_LISTEN_ONLY,   // 21: hardware listen-only (existing USB-logger mode)
  BC_ACTION_TOGGLE_KICKDOWN,      // 22
  BC_ACTION_COUNT
};

// Actions run outside the Body Comfort lock (they persist NVS, take the CAN TX
// barrier or wait for the CAN supervisor).
static inline bool bcActionIsExistingFeaturePure(uint8_t action) {
  return (action >= BC_ACTION_TOGGLE_NAG_KILLER && action <= BC_ACTION_TOGGLE_BATTERY_PREHEAT) ||
         action == BC_ACTION_TOGGLE_LISTEN_ONLY;
}

enum BcRearLightModePure : uint8_t {
  BC_REAR_LIGHT_FOG = 0,
  BC_REAR_LIGHT_LIGHTS_ON = 1,
  BC_REAR_LIGHT_BOTH = 2,
};

struct BodyComfortConfigPure {
  // 1) Easy Entry
  bool eeEnabled;
  uint8_t eeDoorMask;        // bit0 FL bit1 FR bit2 RL bit3 RR
  bool eeWeatherBlock;       // rain / snow blocks the window drop
  bool eeWeatherStrict;      // missing weather data also blocks
  uint16_t eeRestoreDelayMs; // after the door latches closed
  bool eeOpenPercent;        // open with GOTO_PERCENT 100 instead of GOTO_OPEN
  bool eeMirrorFold;         // fold mirrors when Easy Entry acts
  bool eeMirrorCamera;       // show the camera while mirrors are folded by us
  uint8_t eeUnfoldKph;       // unfold above this speed in D ...
  uint8_t eeUnfoldHoldS;     // ... for at least this long
  // 2) Rear seat-belt chime
  bool sbEnabled;
  uint8_t sbSeatMask;        // bit0 2nd-row left, bit1 centre, bit2 right
  // 3) Steering-wheel macros
  bool mcEnabled;
  uint16_t mcLongMs;
  uint16_t mcGapMs;
  uint8_t mcAction[BC_BTN_COUNT][BC_GESTURE_COUNT];
  // 4) Hazard blink: one-shot, hazards on for hzBlinkSeconds then off
  bool hzBlinkEnabled;
  uint8_t hzBlinkSeconds;
  // 5) Reverse hazards
  bool rvHazard;
  // 6) Reverse rear light
  bool rvLight;
  uint8_t rvLightMode;
  // 7) D/R door-open hazards
  bool drHazard;
  uint16_t drOffDelayMs;
  // 8) Kickdown: pedal >= trigger switches UI_pedalMap to the target mode
  bool kdEnabled;
  uint8_t kdTriggerPct;     // 10..100
  uint8_t kdTargetRaw;      // 1 Standard, 2 Sport / Performance
  bool kdReleaseOnPedal;
  uint8_t kdReleasePct;     // 0..90, always below the trigger
  bool kdReleaseOnBrake;
  uint8_t kdReleaseMode;    // 0 Comfort (Chill), 1 vehicle setting (stock)
  // 9) Door-handle macro: hold the exterior handle to run an action
  bool dhEnabled;
  uint8_t dhDoorMask;       // bit0 driver (front left), bit1 passenger (front right)
  uint8_t dhHoldS;          // 1..10
  uint8_t dhAction;         // BcActionPure, default Open frunk
};

static constexpr uint8_t BC_KD_TARGET_STANDARD = 1u;
static constexpr uint8_t BC_KD_TARGET_SPORT = 2u;
static constexpr uint8_t BC_KD_RELEASE_COMFORT = 0u;
static constexpr uint8_t BC_KD_RELEASE_STOCK = 1u;

static inline BodyComfortConfigPure bodyComfortDefaultsPure() {
  BodyComfortConfigPure c = {};
  c.eeDoorMask = 0x0Fu;
  c.eeWeatherBlock = true;
  c.eeRestoreDelayMs = 800u;
  c.eeMirrorCamera = true;
  c.eeUnfoldKph = 10u;
  c.eeUnfoldHoldS = 3u;
  c.sbSeatMask = 0x07u;
  c.mcLongMs = 800u;
  c.mcGapMs = 400u;
  c.hzBlinkSeconds = 3u;
  c.rvLightMode = BC_REAR_LIGHT_BOTH;
  c.drOffDelayMs = 2000u;
  c.kdTriggerPct = 90u;
  c.kdTargetRaw = BC_KD_TARGET_STANDARD;
  c.kdReleaseOnPedal = true;
  c.kdReleasePct = 20u;
  c.kdReleaseOnBrake = true;
  c.kdReleaseMode = BC_KD_RELEASE_COMFORT;
  c.dhDoorMask = 0x01u;
  c.dhHoldS = 2u;
  c.dhAction = BC_ACTION_OPEN_FRUNK;
  return c;
}

template <typename T>
static inline T bcClampPure(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline void bodyComfortSanitizePure(BodyComfortConfigPure &c) {
  c.eeDoorMask &= 0x0Fu;
  c.eeRestoreDelayMs = bcClampPure<uint16_t>(c.eeRestoreDelayMs, 0u, 5000u);
  c.eeUnfoldKph = bcClampPure<uint8_t>(c.eeUnfoldKph, 1u, 80u);
  c.eeUnfoldHoldS = bcClampPure<uint8_t>(c.eeUnfoldHoldS, 0u, 30u);
  c.sbSeatMask &= 0x07u;
  c.mcLongMs = bcClampPure<uint16_t>(c.mcLongMs, 500u, 2000u);
  c.mcGapMs = bcClampPure<uint16_t>(c.mcGapMs, 200u, 800u);
  for (uint8_t b = 0; b < BC_BTN_COUNT; ++b)
    for (uint8_t g = 0; g < BC_GESTURE_COUNT; ++g)
      if (c.mcAction[b][g] >= BC_ACTION_COUNT || c.mcAction[b][g] == BC_ACTION_RETIRED_4)
        c.mcAction[b][g] = BC_ACTION_NONE;
  c.hzBlinkSeconds = bcClampPure<uint8_t>(c.hzBlinkSeconds, 1u, 30u);
  if (c.rvLightMode > BC_REAR_LIGHT_BOTH) c.rvLightMode = BC_REAR_LIGHT_BOTH;
  c.drOffDelayMs = bcClampPure<uint16_t>(c.drOffDelayMs, 0u, 10000u);
  c.kdTriggerPct = bcClampPure<uint8_t>(c.kdTriggerPct, 10u, 100u);
  if (c.kdTargetRaw != BC_KD_TARGET_STANDARD && c.kdTargetRaw != BC_KD_TARGET_SPORT)
    c.kdTargetRaw = BC_KD_TARGET_STANDARD;
  c.kdReleasePct = bcClampPure<uint8_t>(c.kdReleasePct, 0u, 90u);
  if (c.kdReleasePct + 5u > c.kdTriggerPct) c.kdReleasePct = (uint8_t)(c.kdTriggerPct - 5u);
  if (!c.kdReleaseOnPedal && !c.kdReleaseOnBrake) c.kdReleaseOnPedal = true;  // always a way back
  if (c.kdReleaseMode > BC_KD_RELEASE_STOCK) c.kdReleaseMode = BC_KD_RELEASE_COMFORT;
  c.dhDoorMask &= 0x03u;
  c.dhHoldS = bcClampPure<uint8_t>(c.dhHoldS, 1u, 10u);
  if (c.dhAction >= BC_ACTION_COUNT || c.dhAction == BC_ACTION_RETIRED_4) c.dhAction = BC_ACTION_NONE;
}

// v1: 36 bytes, six wheel controls. v2: 42 bytes, adds voice + camera buttons.
// v3: 50 bytes, adds kickdown and the door-handle macro after the macro table.
static constexpr uint8_t BC_CFG_BLOB_VERSION = 3u;
static constexpr uint8_t BC_CFG_BLOB_LEN_V1 = 17u + BC_BTN_COUNT_V1 * BC_GESTURE_COUNT + 1u;
static constexpr uint8_t BC_CFG_BLOB_LEN_V2 = 17u + (uint8_t)BC_BTN_COUNT * (uint8_t)BC_GESTURE_COUNT + 1u;
static constexpr uint8_t BC_CFG_BLOB_V3_EXTRA = 8u;
static constexpr uint8_t BC_CFG_BLOB_LEN = BC_CFG_BLOB_LEN_V2 + BC_CFG_BLOB_V3_EXTRA;
static_assert(BC_CFG_BLOB_LEN_V1 == 36u && BC_CFG_BLOB_LEN_V2 == 42u && BC_CFG_BLOB_LEN == 50u,
              "config blob layout");

static inline uint8_t bcCfgBlobChecksumLenPure(const uint8_t *b, uint8_t len) {
  uint8_t sum = 0xA5u;
  for (uint8_t i = 0; i + 1u < len; ++i) sum = (uint8_t)(sum + b[i]);
  return sum;
}

static inline uint8_t bcCfgBlobChecksumPure(const uint8_t *b) {
  return bcCfgBlobChecksumLenPure(b, BC_CFG_BLOB_LEN);
}

static inline void bodyComfortSerializePure(const BodyComfortConfigPure &c,
                                            uint8_t out[BC_CFG_BLOB_LEN]) {
  memset(out, 0, BC_CFG_BLOB_LEN);
  out[0] = BC_CFG_BLOB_VERSION;
  out[1] = (uint8_t)((c.eeEnabled ? 0x01u : 0u) | (c.eeWeatherBlock ? 0x02u : 0u) |
                     (c.eeWeatherStrict ? 0x04u : 0u) | (c.eeMirrorFold ? 0x08u : 0u) |
                     (c.eeMirrorCamera ? 0x10u : 0u) | (c.sbEnabled ? 0x20u : 0u) |
                     (c.mcEnabled ? 0x40u : 0u) | (c.hzBlinkEnabled ? 0x80u : 0u));
  out[2] = (uint8_t)((c.rvHazard ? 0x01u : 0u) | (c.rvLight ? 0x02u : 0u) |
                     (c.drHazard ? 0x04u : 0u) | (c.eeOpenPercent ? 0x08u : 0u));
  out[3] = c.eeDoorMask;
  out[4] = (uint8_t)(c.eeRestoreDelayMs & 0xFFu);
  out[5] = (uint8_t)(c.eeRestoreDelayMs >> 8);
  out[6] = c.eeUnfoldKph;
  out[7] = c.eeUnfoldHoldS;
  out[8] = c.sbSeatMask;
  out[9] = (uint8_t)(c.mcLongMs & 0xFFu);
  out[10] = (uint8_t)(c.mcLongMs >> 8);
  out[11] = (uint8_t)(c.mcGapMs & 0xFFu);
  out[12] = (uint8_t)(c.mcGapMs >> 8);
  out[13] = c.hzBlinkSeconds;
  out[14] = c.rvLightMode;
  out[15] = (uint8_t)(c.drOffDelayMs & 0xFFu);
  out[16] = (uint8_t)(c.drOffDelayMs >> 8);
  uint8_t i = 17u;
  for (uint8_t b = 0; b < BC_BTN_COUNT; ++b)
    for (uint8_t g = 0; g < BC_GESTURE_COUNT; ++g) out[i++] = c.mcAction[b][g];
  out[i++] = (uint8_t)((c.kdEnabled ? 0x01u : 0u) | (c.kdReleaseOnPedal ? 0x02u : 0u) |
                       (c.kdReleaseOnBrake ? 0x04u : 0u) | (c.dhEnabled ? 0x08u : 0u));
  out[i++] = c.kdTriggerPct;
  out[i++] = c.kdTargetRaw;
  out[i++] = c.kdReleasePct;
  out[i++] = c.kdReleaseMode;
  out[i++] = c.dhDoorMask;
  out[i++] = c.dhHoldS;
  out[i++] = c.dhAction;
  out[BC_CFG_BLOB_LEN - 1u] = bcCfgBlobChecksumPure(out);
}

static inline bool bodyComfortDeserializePure(const uint8_t *in, uint32_t len,
                                              BodyComfortConfigPure &c) {
  c = bodyComfortDefaultsPure();
  if (in == nullptr) return false;
  uint8_t buttons = 0;
  bool hasV3 = false;
  if (len == BC_CFG_BLOB_LEN && in[0] == BC_CFG_BLOB_VERSION) { buttons = BC_BTN_COUNT; hasV3 = true; }
  else if (len == BC_CFG_BLOB_LEN_V2 && in[0] == 2u) buttons = BC_BTN_COUNT;      // migrate v2
  else if (len == BC_CFG_BLOB_LEN_V1 && in[0] == 1u) buttons = BC_BTN_COUNT_V1;  // migrate v1
  if (buttons == 0u || bcCfgBlobChecksumLenPure(in, (uint8_t)len) != in[len - 1u]) return false;
  c.eeEnabled = (in[1] & 0x01u) != 0u;
  c.eeWeatherBlock = (in[1] & 0x02u) != 0u;
  c.eeWeatherStrict = (in[1] & 0x04u) != 0u;
  c.eeMirrorFold = (in[1] & 0x08u) != 0u;
  c.eeMirrorCamera = (in[1] & 0x10u) != 0u;
  c.sbEnabled = (in[1] & 0x20u) != 0u;
  c.mcEnabled = (in[1] & 0x40u) != 0u;
  c.hzBlinkEnabled = (in[1] & 0x80u) != 0u;
  c.rvHazard = (in[2] & 0x01u) != 0u;
  c.rvLight = (in[2] & 0x02u) != 0u;
  c.drHazard = (in[2] & 0x04u) != 0u;
  c.eeOpenPercent = (in[2] & 0x08u) != 0u;
  c.eeDoorMask = in[3];
  c.eeRestoreDelayMs = (uint16_t)(in[4] | (in[5] << 8));
  c.eeUnfoldKph = in[6];
  c.eeUnfoldHoldS = in[7];
  c.sbSeatMask = in[8];
  c.mcLongMs = (uint16_t)(in[9] | (in[10] << 8));
  c.mcGapMs = (uint16_t)(in[11] | (in[12] << 8));
  c.hzBlinkSeconds = in[13];
  c.rvLightMode = in[14];
  c.drOffDelayMs = (uint16_t)(in[15] | (in[16] << 8));
  uint8_t i = 17u;
  for (uint8_t b = 0; b < buttons; ++b)
    for (uint8_t g = 0; g < BC_GESTURE_COUNT; ++g) c.mcAction[b][g] = in[i++];
  if (hasV3) {
    c.kdEnabled = (in[i] & 0x01u) != 0u;
    c.kdReleaseOnPedal = (in[i] & 0x02u) != 0u;
    c.kdReleaseOnBrake = (in[i] & 0x04u) != 0u;
    c.dhEnabled = (in[i] & 0x08u) != 0u;
    c.kdTriggerPct = in[i + 1u];
    c.kdTargetRaw = in[i + 2u];
    c.kdReleasePct = in[i + 3u];
    c.kdReleaseMode = in[i + 4u];
    c.dhDoorMask = in[i + 5u];
    c.dhHoldS = in[i + 6u];
    c.dhAction = in[i + 7u];
  }
  bodyComfortSanitizePure(c);
  return true;
}

// Applying a toggle action from a macro. Returns true when the config changed.
static inline bool bodyComfortApplyToggleActionPure(BodyComfortConfigPure &c, uint8_t action) {
  switch (action) {
    case BC_ACTION_TOGGLE_EASY_ENTRY: c.eeEnabled = !c.eeEnabled; return true;
    case BC_ACTION_TOGGLE_MIRROR_FOLD_OPT: c.eeMirrorFold = !c.eeMirrorFold; return true;
    case BC_ACTION_TOGGLE_SEATBELT: c.sbEnabled = !c.sbEnabled; return true;
    case BC_ACTION_TOGGLE_REVERSE_HAZARD: c.rvHazard = !c.rvHazard; return true;
    case BC_ACTION_TOGGLE_REVERSE_LIGHT: c.rvLight = !c.rvLight; return true;
    case BC_ACTION_TOGGLE_DOOR_HAZARD: c.drHazard = !c.drHazard; return true;
    case BC_ACTION_TOGGLE_KICKDOWN: c.kdEnabled = !c.kdEnabled; return true;
    default: return false;
  }
}

// ─── Kickdown ────────────────────────────────────────────────────────────
// Engage: D, fresh pedal at/above the trigger for BC_KD_ENGAGE_MS, brake off.
// Release (any enabled condition): brake pressed, pedal at/below the release
// level for BC_KD_RELEASE_MS, leaving D, or the feature switched off.
static constexpr uint32_t BC_KD_ENGAGE_MS = 100u;
static constexpr uint32_t BC_KD_RELEASE_MS = 500u;
static constexpr uint32_t BC_KD_RETRY_MS = 300u;

enum BcKickdownActionPure : uint8_t {
  BC_KD_ACTION_NONE = 0,
  BC_KD_ACTION_ENGAGE,
  BC_KD_ACTION_RELEASE,
};

struct BcKickdownPure {
  bool kicked;
  bool aboveTiming;
  uint32_t aboveSinceMs;
  bool belowTiming;
  uint32_t belowSinceMs;
  uint32_t retryAfterMs;
  bool retryPending;
};

struct BcKickdownInputPure {
  bool enabled;
  bool gearD;
  bool pedalValid;
  uint16_t pedalPct10;
  bool brakeValid;
  bool brakeOn;
};

static inline uint8_t bcKickdownStepPure(BcKickdownPure &k, uint32_t now,
                                         const BcKickdownInputPure &in,
                                         const BodyComfortConfigPure &c) {
  if (k.retryPending && (int32_t)(now - k.retryAfterMs) < 0) return BC_KD_ACTION_NONE;
  k.retryPending = false;
  if (k.kicked) {
    bool release = !in.enabled || !in.gearD;
    if (!release && c.kdReleaseOnBrake && in.brakeValid && in.brakeOn) release = true;
    if (!release && c.kdReleaseOnPedal && in.pedalValid &&
        in.pedalPct10 <= (uint16_t)c.kdReleasePct * 10u) {
      if (!k.belowTiming) { k.belowTiming = true; k.belowSinceMs = now; }
      if ((uint32_t)(now - k.belowSinceMs) >= BC_KD_RELEASE_MS) release = true;
    } else {
      k.belowTiming = false;
    }
    if (!release) return BC_KD_ACTION_NONE;
    k.kicked = false;
    k.belowTiming = false;
    k.aboveTiming = false;
    return BC_KD_ACTION_RELEASE;
  }
  const bool above = in.enabled && in.gearD && in.pedalValid &&
                     in.pedalPct10 >= (uint16_t)c.kdTriggerPct * 10u &&
                     !(in.brakeValid && in.brakeOn);
  if (!above) { k.aboveTiming = false; return BC_KD_ACTION_NONE; }
  if (!k.aboveTiming) { k.aboveTiming = true; k.aboveSinceMs = now; }
  if ((uint32_t)(now - k.aboveSinceMs) < BC_KD_ENGAGE_MS) return BC_KD_ACTION_NONE;
  k.kicked = true;
  k.aboveTiming = false;
  k.belowTiming = false;
  return BC_KD_ACTION_ENGAGE;
}

// The pedal-map request was refused: undo the transition and retry later. A
// release outside D is not retried: entering P already resets the pedal map.
static inline void bcKickdownRequestFailedPure(BcKickdownPure &k, uint32_t now, uint8_t action,
                                               bool gearD) {
  if (action == BC_KD_ACTION_ENGAGE) k.kicked = false;
  else if (action == BC_KD_ACTION_RELEASE) k.kicked = gearD;
  k.retryPending = true;
  k.retryAfterMs = now + BC_KD_RETRY_MS;
}

// ─── Wi-Fi access-point timing ───────────────────────────────────────────
struct WifiApTimingPure {
  bool slowStartEnabled;
  uint16_t startDelayS;  // 0..600
  bool hideEnabled;
  uint16_t hideDelayS;   // 0..3600, counted from AP start
};

static inline WifiApTimingPure wifiApTimingDefaultsPure() {
  WifiApTimingPure t = {};
  t.startDelayS = 30u;
  t.hideDelayS = 300u;
  return t;
}

static inline void wifiApTimingSanitizePure(WifiApTimingPure &t) {
  if (t.startDelayS > 600u) t.startDelayS = 600u;
  if (t.hideDelayS > 3600u) t.hideDelayS = 3600u;
}

// Start delay applies only to a configured vehicle. Profile setup / NVS error
// mode always starts immediately so the device stays recoverable.
static inline uint32_t wifiApStartDelayMsPure(const WifiApTimingPure &t, bool setupMode) {
  if (setupMode || !t.slowStartEnabled) return 0u;
  return (uint32_t)(t.startDelayS > 600u ? 600u : t.startDelayS) * 1000u;
}

static inline bool wifiApHideDuePure(const WifiApTimingPure &t, bool setupMode,
                                     bool apStarted, uint32_t now, uint32_t apStartMs) {
  if (setupMode || !t.hideEnabled || !apStarted) return false;
  return (uint32_t)(now - apStartMs) >= (uint32_t)t.hideDelayS * 1000u;
}
