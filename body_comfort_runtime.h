#pragma once

// Body Comfort runtime (add_feats branch).
//
// Features (each individually switchable, all OFF by default):
//   1) Easy Entry          — in P, a door opening drops its window fully; the
//                            window returns to its previous position after the
//                            door closes. Rain/snow blocks the drop. Optional
//                            mirror fold + camera while folded + auto unfold.
//   2) Rear belt chime off — per 2nd-row seat suppression of the
//                            occupied-and-unbuckled summary.
//   3) Steering macros     — double / triple / long press on six wheel controls.
//   4) Hazard blink        — blink the hazards N times.
//   5) Reverse hazards     — hazards while in R.
//   6) Reverse rear light  — rear fog and/or lights ON while in R.
//   7) Door-open hazards   — a door opened in D/R turns hazards on until all
//                            doors are closed plus a delay.
//
// Route: Standard Body + Chassis (Juniper) uses Body CAN A. Model Y L uses the
// same VEH frames on VH CAN B. Party + Chassis and Model S are unsupported.
// Every transmitted frame is a fresh stock frame with only documented fields
// changed, sent through the shared CAN TX recovery barrier.

#include "body_comfort_pure.h"

enum BodyComfortBusRuntime : uint8_t { BC_BUS_NONE = 0, BC_BUS_A = 1, BC_BUS_B = 2 };

static inline uint8_t bodyComfortActiveBus() {
  if (vehicleProfileSetupMode || vehicleProfileNvsError) return BC_BUS_NONE;
  if (activeProfileIsModelS2017()) return BC_BUS_NONE;
  if (activeCanAIsBody()) return BC_BUS_A;
  if (activeProfileIsYl() && activeVehicleTopology == VEHICLE_TOPOLOGY_YL_PARTY_VH)
    return BC_BUS_B;
  return BC_BUS_NONE;
}

static const char *bodyComfortBusName(uint8_t bus) {
  if (bus == BC_BUS_A) return "BODY_CAN_A";
  if (bus == BC_BUS_B) return "VH_CAN_B";
  return "NONE";
}

// ─── State ───────────────────────────────────────────────────────────────
static constexpr const char *BC_NVS_NAMESPACE = "bodycmf";
static constexpr const char *BC_NVS_KEY = "cfg";
static constexpr uint32_t BC_ENTRY_SNAPSHOT_SETTLE_MS = 1500u;
static constexpr uint32_t BC_CFG_SAVE_DEBOUNCE_MS = 1500u;
static constexpr uint8_t BC_MACRO_QUEUE_LEN = 4u;
static constexpr uint8_t BC_CAMERA_MAX_ATTEMPTS = 2u;
static constexpr uint32_t BC_CAMERA_RETRY_MS = 3000u;
static constexpr uint32_t BC_MIRROR_USER_UNFOLD_GRACE_MS = 8000u;

enum BcEntryPhaseRuntime : uint8_t {
  BC_ENTRY_IDLE = 0,
  BC_ENTRY_OPEN_PENDING,
  BC_ENTRY_OPENED,
  BC_ENTRY_RESTORE_PENDING,
};

struct BcEntryDoorRuntime {
  uint8_t phase;
  bool inFlight;
  bool closeSeen;
  uint8_t attempts;
  uint32_t dueMs;
  BcRestorePlanPure plan;
  uint8_t lastDoor;
  // Settled pre-entry window snapshot.
  bool snapValid;
  uint8_t snapPosState;
  int16_t snapPosRaw;
  uint8_t candPosState;
  uint32_t candSinceMs;
  BcWindowLearnPure learn;
};

struct BcOneShotRuntime {
  bool active;
  uint8_t framesLeft;
  uint32_t startMs;
};

struct BcPressRuntime {
  bool active;
  uint32_t pressUntilMs;
  uint32_t busyUntilMs;
  uint32_t framesSent;
};

struct BcMacroEventRuntime {
  uint8_t button;
  uint8_t gesture;
};

struct BcStateRuntime {
  // Observations (body bus).
  uint8_t gear; uint32_t gearMs; bool gearValid;
  int32_t kph10; uint32_t speedMs; bool speedValid;
  bool rainy, snowy; uint32_t weatherMs; bool weatherValid;
  bool cameraActive; uint32_t uiStatusMs; bool uiStatusValid;
  uint8_t hazardReq; bool indicatorOn; uint32_t lightingMs; bool lightingValid;
  uint8_t door[BC_DOOR_COUNT];
  uint32_t doorMs[2]; bool doorValid[2];
  uint8_t mirrorFold[2];
  uint8_t winMotion[BC_DOOR_COUNT]; int16_t winPos[BC_DOOR_COUNT];
  uint32_t winM0Ms[2]; bool winM0Valid[2];
  uint8_t winPosState[BC_DOOR_COUNT];
  uint32_t winM1Ms[2]; bool winM1Valid[2];
  bool winSwitch[BC_DOOR_COUNT]; uint32_t switchM0Ms; bool switchM0Valid;
  bool hazardPhysical;
  uint16_t pedalPct10; uint32_t pedalMs; bool pedalValid;
  bool brakeOn; uint32_t brakeMs; bool brakeValid;
  bool handlePulled[2];             // 0 driver (0x102), 1 passenger (0x103)
  BcLockFramePure lock; uint32_t lockMs; bool lockValid;
  uint32_t handleBlockedLocked, handleBlockedClosed;
  bool handleBlockNoted[2];

  // Kickdown.
  BcKickdownPure kickdown;
  uint32_t kickdownEngages, kickdownReleases, kickdownRefused;

  // Door-handle macro.
  bool handleTiming[2];
  uint32_t handleSinceMs[2];
  bool handleFired[2];

  // Frunk.
  BcOneShotRuntime frunkCmd;
  uint32_t frunkOpens, frunkRefused;

  // Easy Entry.
  BcEntryDoorRuntime entry[BC_DOOR_COUNT];
  BcOneShotRuntime winCmd;
  uint8_t winCmdMask, winCmdRequest, winCmdPct;
  uint8_t winCmdStockSent, winCmdSynthSent;
  uint32_t winCmdLastSynthMs;
  bool winCmdStockSeen;
  // Newest stock 0x294 (template for synthesized window commands).
  uint8_t vc3Template[8]; uint32_t vc3Ms; bool vc3Valid; uint32_t vc3RxCount;
  // Last finished window command, for diagnostics.
  uint8_t lastWinRequest, lastWinMask, lastWinStock, lastWinSynth, lastWinOutcome;
  uint32_t lastWinMs;
  uint8_t winUnavail[BC_DOOR_COUNT], winStop[BC_DOOR_COUNT];
  // Window requests seen in stock 0x294 (what the touchscreen itself sends).
  uint8_t vc3Seen[4][8]; uint32_t vc3SeenMs[4]; uint8_t vc3SeenHead, vc3SeenCount;
  uint32_t vc3WindowFrames;

  // Mirrors / camera.
  BcOneShotRuntime mirrorCmd;
  uint8_t mirrorCmdRequest;
  bool mirrorFoldedByUs;
  uint32_t mirrorFoldMs;
  BcUnfoldTimerPure unfoldTimer;
  bool cameraOpenedByUs;
  uint8_t cameraAttempts;
  uint32_t cameraLastPressMs;
  bool closeCameraAfterUnfold;

  // Hazards.
  BcHazardCtlPure hazardCtl;
  BcPressRuntime hazardPress;
  BcPressRuntime cameraPress;
  bool blinkActive;
  uint32_t blinkStartMs;
  bool blinkOnSeen;
  uint32_t blinkOnMs;
  bool blinkLastIndicator;
  bool reverseHazardWanted;
  BcDoorHazardPure doorHazard;
  bool doorHazardWanted;
  bool rearLightActive;

  // Macros.
  BcGesturePure gesture[BC_BTN_COUNT];
  BcMacroEventRuntime macroQueue[BC_MACRO_QUEUE_LEN];
  uint8_t macroHead, macroCount;
  uint8_t lastMacroButton, lastMacroGesture, lastMacroAction;
  const char *lastMacroResult;
  uint32_t lastMacroMs;
  uint32_t macroEvents, macroDropped;

  // Statistics.
  uint32_t txOk, txFail;
  uint32_t entryOpens, entryRestores, entryBlockedWeather, entryBlockedGear;
  uint32_t entryCancelledSwitch, winCmdTimeouts;
  uint32_t mirrorFolds, mirrorUnfolds, mirrorCmdTimeouts, cameraPresses;
  uint32_t hazardPresses, blinkRuns;
  uint32_t seatbeltInjected, seatbeltBadChecksum;
  uint32_t rearLightInjected, rearLightReinforced, rearLightTxFail;
  uint8_t vc273Template[8]; uint32_t vc273Ms; bool vc273Valid;
  uint8_t vc3b3Template[8]; uint32_t vc3b3Ms; bool vc3b3Valid;
  uint32_t fogLastTxMs, lightLastTxMs;
};

static portMUX_TYPE bcMux = portMUX_INITIALIZER_UNLOCKED;
static BodyComfortConfigPure bcCfg = bodyComfortDefaultsPure();   // bcMux
static BcStateRuntime bcState = {};                                // bcMux
static bool bcCfgDirty = false;                                    // bcMux
static uint32_t bcCfgDirtyMs = 0;                                  // bcMux
static uint32_t bcCfgSaveFailures = 0;                            // bcMux
static TaskHandle_t bodyComfortTaskHandle = nullptr;

static BodyComfortConfigPure bodyComfortCfgSnapshot() {
  portENTER_CRITICAL(&bcMux);
  const BodyComfortConfigPure c = bcCfg;
  portEXIT_CRITICAL(&bcMux);
  return c;
}

// ─── NVS ─────────────────────────────────────────────────────────────────
static bool bodyComfortCfgPersist(const BodyComfortConfigPure &c) {
  uint8_t blob[BC_CFG_BLOB_LEN];
  bodyComfortSerializePure(c, blob);
  Preferences p;
  if (!p.begin(BC_NVS_NAMESPACE, false)) return false;
  const size_t written = p.putBytes(BC_NVS_KEY, blob, sizeof(blob));
  p.end();
  return written == sizeof(blob);
}

static void bodyComfortCfgLoad() {
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  Preferences p;
  if (p.begin(BC_NVS_NAMESPACE, true)) {
    uint8_t blob[BC_CFG_BLOB_LEN] = {};
    const size_t len = p.getBytesLength(BC_NVS_KEY);
    if ((len == BC_CFG_BLOB_LEN || len == BC_CFG_BLOB_LEN_V1) &&
        p.getBytes(BC_NVS_KEY, blob, len) == len) {
      if (!bodyComfortDeserializePure(blob, (uint32_t)len, c)) c = bodyComfortDefaultsPure();
    }
    p.end();
  }
  portENTER_CRITICAL(&bcMux);
  bcCfg = c;
  bcCfgDirty = false;
  portEXIT_CRITICAL(&bcMux);
  T2CAN_SERIAL_PRINTF("Body Comfort: EE=%u belt=%u macro=%u blink=%u revHaz=%u revLight=%u doorHaz=%u\n",
                      c.eeEnabled, c.sbEnabled, c.mcEnabled, c.hzBlinkEnabled,
                      c.rvHazard, c.rvLight, c.drHazard);
}

// ─── TX ──────────────────────────────────────────────────────────────────
static bool bodyComfortTransmit(uint8_t bus, uint16_t id, const uint8_t *data, uint8_t dlc) {
  if (data == nullptr || dlc > 8u) return false;
  // Listen-only: observation continues (macros can switch it back), TX never.
  if (canUsbTxHeld()) return false;
  const uint32_t epoch = canTxEpochSnapshot();
  bool ok = false;
  if (bus == BC_BUS_A) {
    struct can_frame f = {};
    f.can_id = id;
    f.can_dlc = dlc;
    memcpy(f.data, data, dlc);
    MCP2515::ERROR err = MCP2515::ERROR_FAIL;
    const bool attempted =
        canTxMcpSendTagged(&f, epoch, CAN_TX_TRACE_SOURCE_BODY_COMFORT, err, nullptr);
    ok = attempted && err == MCP2515::ERROR_OK;
  } else if (bus == BC_BUS_B) {
    twai_message_t m = {};
    m.identifier = id;
    m.data_length_code = dlc;
    memcpy(m.data, data, dlc);
    ok = canTxTwaiTransmitTagged(&m, epoch, CAN_TX_TRACE_SOURCE_BODY_COMFORT) == ESP_OK;
  }
  portENTER_CRITICAL(&bcMux);
  if (ok) bcState.txOk++; else bcState.txFail++;
  portEXIT_CRITICAL(&bcMux);
  return ok;
}

// ─── RX (called from the CAN task owning the body bus) ───────────────────
static void bodyComfortObserveDoor(uint8_t side, const uint8_t *d, uint8_t dlc, uint32_t now) {
  BcDoorFramePure f = {};
  if (!bcDecodeDoorFramePure(d, dlc, f)) return;
  portENTER_CRITICAL(&bcMux);
  bcState.door[side == 0u ? BC_DOOR_FL : BC_DOOR_FR] = f.frontState;
  bcState.door[side == 0u ? BC_DOOR_RL : BC_DOOR_RR] = f.rearState;
  bcState.mirrorFold[side] = f.mirrorFold;
  bcState.handlePulled[side] = f.frontHandlePulled;
  bcState.doorMs[side] = now;
  bcState.doorValid[side] = true;
  portEXIT_CRITICAL(&bcMux);
}

static void bodyComfortObserveWindow(uint8_t side, const uint8_t *d, uint8_t dlc, uint32_t now) {
  BcWindowFramePure w = {};
  if (!bcDecodeWindowFramePure(d, dlc, w)) return;
  const uint8_t a = side == 0u ? BC_DOOR_FL : BC_DOOR_FR;
  const uint8_t b = side == 0u ? BC_DOOR_RL : BC_DOOR_RR;
  portENTER_CRITICAL(&bcMux);
  if (w.mux == 0u) {
    bcState.winMotion[a] = w.motionA; bcState.winPos[a] = w.posRawA;
    bcState.winMotion[b] = w.motionB; bcState.winPos[b] = w.posRawB;
    bcState.winM0Ms[side] = now; bcState.winM0Valid[side] = true;
  } else {
    bcState.winPosState[a] = w.posStateA;
    bcState.winPosState[b] = w.posStateB;
    bcState.winUnavail[a] = w.unavailA; bcState.winUnavail[b] = w.unavailB;
    bcState.winStop[a] = w.stopA; bcState.winStop[b] = w.stopB;
    bcState.winM1Ms[side] = now; bcState.winM1Valid[side] = true;
  }
  portEXIT_CRITICAL(&bcMux);
}

static void bodyComfortObserveSwitch(uint8_t bus, const uint8_t *d, uint8_t dlc, uint32_t now) {
  if (dlc < 8u) return;
  const uint8_t mux = bcSwitchMuxPure(d);
  uint8_t out[8];
  memcpy(out, d, 8);
  bool send = false;
  if (mux == 0u) {
    const bool physical = bcHazardButtonPressedPure(d);
    portENTER_CRITICAL(&bcMux);
    for (uint8_t door = 0; door < BC_DOOR_COUNT; ++door)
      bcState.winSwitch[door] = bcWindowSwitchActivePure(d, door);
    bcState.switchM0Ms = now;
    bcState.switchM0Valid = true;
    bcState.hazardPhysical = physical;
    BcPressRuntime &p = bcState.hazardPress;
    if (p.active && (int32_t)(p.pressUntilMs - now) > 0 && !physical) {
      bcSetHazardButtonPure(out, true);
      p.framesSent++;
      send = true;
    }
    portEXIT_CRITICAL(&bcMux);
  } else if (mux == 1u) {
    uint8_t events[BC_BTN_COUNT];
    uint8_t eventCount = 0;
    portENTER_CRITICAL(&bcMux);
    const uint16_t longMs = bcCfg.mcLongMs;
    const bool macros = bcCfg.mcEnabled;
    for (uint8_t b = 0; b < BC_BTN_COUNT; ++b) {
      const uint8_t ev = bcGestureObservePure(bcState.gesture[b], bcButtonPressedPure(d, b), now, longMs);
      if (ev != BC_GESTURE_NONE && macros) events[eventCount++] = (uint8_t)((b << 4) | ev);
    }
    for (uint8_t i = 0; i < eventCount; ++i) {
      if (bcState.macroCount >= BC_MACRO_QUEUE_LEN) { bcState.macroDropped++; continue; }
      const uint8_t slot = (uint8_t)((bcState.macroHead + bcState.macroCount) % BC_MACRO_QUEUE_LEN);
      bcState.macroQueue[slot] = {(uint8_t)(events[i] >> 4), (uint8_t)(events[i] & 0x0Fu)};
      bcState.macroCount++;
    }
    BcPressRuntime &p = bcState.cameraPress;
    if (p.active && (int32_t)(p.pressUntilMs - now) > 0 && !bcCameraButtonPressedPure(d)) {
      bcSetCameraButtonPure(out, true);
      p.framesSent++;
      send = true;
    }
    portEXIT_CRITICAL(&bcMux);
  }
  if (send) bodyComfortTransmit(bus, BC_ID_VCLEFT_SWITCH, out, 8);
}

static void bodyComfortObserveVehicleControl(uint8_t bus, const uint8_t *d, uint8_t dlc, uint32_t now) {
  if (dlc < 8u) return;
  uint8_t out[8];
  memcpy(out, d, 8);
  bool send = false;
  bool fogSent = false;
  portENTER_CRITICAL(&bcMux);
  memcpy(bcState.vc273Template, d, 8);
  bcState.vc273Ms = now;
  bcState.vc273Valid = true;
  BcOneShotRuntime &m = bcState.mirrorCmd;
  if (m.active && m.framesLeft > 0u && (uint32_t)(now - m.startMs) <= BC_ONE_SHOT_TIMEOUT_MS) {
    bcSetMirrorFoldRequestPure(out, bcState.mirrorCmdRequest);
    m.framesLeft--;
    send = true;
  }
  BcOneShotRuntime &fr = bcState.frunkCmd;
  if (fr.active && fr.framesLeft > 0u && (uint32_t)(now - fr.startMs) <= BC_ONE_SHOT_TIMEOUT_MS) {
    bcSetFrunkRequestPure(out);
    fr.framesLeft--;
    send = true;
  }
  if (bcState.rearLightActive && bcCfg.rvLightMode != BC_REAR_LIGHT_LIGHTS_ON &&
      bcGetBitsPure(d, 23, 1) == 0u) {
    bcSetRearFogPure(out, true);
    send = true;
    fogSent = true;
  }
  if (fogSent) bcState.rearLightInjected++;
  portEXIT_CRITICAL(&bcMux);
  if (!send) return;
  const bool ok = bodyComfortTransmit(bus, BC_ID_UI_VEHICLE_CONTROL, out, 8);
  if (fogSent) {
    portENTER_CRITICAL(&bcMux);
    if (ok) bcState.fogLastTxMs = now; else bcState.rearLightTxFail++;
    portEXIT_CRITICAL(&bcMux);
  }
}

static void bodyComfortObserveVehicleControl2(uint8_t bus, const uint8_t *d, uint8_t dlc, uint32_t now) {
  if (dlc < 8u) return;
  bool send = false;
  portENTER_CRITICAL(&bcMux);
  memcpy(bcState.vc3b3Template, d, 8);
  bcState.vc3b3Ms = now;
  bcState.vc3b3Valid = true;
  send = bcState.rearLightActive && bcCfg.rvLightMode != BC_REAR_LIGHT_FOG &&
         bcGetBitsPure(d, 9, 3) != BC_LIGHT_SWITCH_ON;
  if (send) bcState.rearLightInjected++;
  portEXIT_CRITICAL(&bcMux);
  if (!send) return;
  uint8_t out[8];
  memcpy(out, d, 8);
  bcSetLightSwitchPure(out, BC_LIGHT_SWITCH_ON);
  const bool ok = bodyComfortTransmit(bus, BC_ID_UI_VEHICLE_CONTROL2, out, 8);
  portENTER_CRITICAL(&bcMux);
  if (ok) bcState.lightLastTxMs = now; else bcState.rearLightTxFail++;
  portEXIT_CRITICAL(&bcMux);
}

// Reverse rear light: re-send the overridden copy between stock frames so the
// override stays the newest value even when a stock frame or a send is missed.
// Built from the newest stock frame (<= 300 ms) with one-shot requests cleared.
static void bcRearLightReinforceLocked(uint32_t now, uint8_t fog[8], bool &fogOut,
                                       uint8_t light[8], bool &lightOut) {
  fogOut = lightOut = false;
  if (!bcState.rearLightActive) return;
  const bool wantFog = bcCfg.rvLightMode != BC_REAR_LIGHT_LIGHTS_ON;
  const bool wantLight = bcCfg.rvLightMode != BC_REAR_LIGHT_FOG;
  if (wantFog && bcState.vc273Valid &&
      (uint32_t)(now - bcState.vc273Ms) <= BC_OVERRIDE_TEMPLATE_MAX_AGE_MS &&
      (uint32_t)(now - bcState.fogLastTxMs) >= BC_OVERRIDE_REINFORCE_MS &&
      !bcState.mirrorCmd.active && !bcState.frunkCmd.active) {
    memcpy(fog, bcState.vc273Template, 8);
    bcClearVc273TransientPure(fog);
    bcSetRearFogPure(fog, true);
    bcState.fogLastTxMs = now;
    fogOut = true;
  }
  if (wantLight && bcState.vc3b3Valid &&
      (uint32_t)(now - bcState.vc3b3Ms) <= BC_OVERRIDE_TEMPLATE_MAX_AGE_MS &&
      (uint32_t)(now - bcState.lightLastTxMs) >= BC_OVERRIDE_REINFORCE_MS) {
    memcpy(light, bcState.vc3b3Template, 8);
    bcClearVc2TransientPure(light);
    bcSetLightSwitchPure(light, BC_LIGHT_SWITCH_ON);
    bcState.lightLastTxMs = now;
    lightOut = true;
  }
  if (fogOut || lightOut) bcState.rearLightReinforced++;
}

static void bodyComfortObserveVehicleControl3(uint8_t bus, const uint8_t *d, uint8_t dlc, uint32_t now) {
  if (dlc < 8u) return;
  uint8_t out[8];
  memcpy(out, d, 8);
  bool send = false;
  portENTER_CRITICAL(&bcMux);
  memcpy(bcState.vc3Template, d, 8);
  bcState.vc3Ms = now;
  bcState.vc3Valid = true;
  bcState.vc3RxCount++;
  if (bcGetBitsPure(d, 52, 3) != BC_WINREQ_IDLE) {
    // Keep the last 4 distinct stock window requests for comparison.
    const uint8_t last = (uint8_t)((bcState.vc3SeenHead + 3u) % 4u);
    if (bcState.vc3SeenCount == 0u || memcmp(bcState.vc3Seen[last], d, 8) != 0) {
      memcpy(bcState.vc3Seen[bcState.vc3SeenHead], d, 8);
      bcState.vc3SeenMs[bcState.vc3SeenHead] = now;
      bcState.vc3SeenHead = (uint8_t)((bcState.vc3SeenHead + 1u) % 4u);
      if (bcState.vc3SeenCount < 4u) bcState.vc3SeenCount++;
    } else {
      bcState.vc3SeenMs[last] = now;
    }
    bcState.vc3WindowFrames++;
  }
  BcOneShotRuntime &w = bcState.winCmd;
  if (w.active && w.framesLeft > 0u && (uint32_t)(now - w.startMs) <= BC_ONE_SHOT_TIMEOUT_MS) {
    bcState.winCmdStockSeen = true;
    send = bcComposeWindowRequestPure(out, bcState.winCmdMask, bcState.winCmdRequest, bcState.winCmdPct);
    if (send) { w.framesLeft--; bcState.winCmdStockSent++; }
  }
  portEXIT_CRITICAL(&bcMux);
  if (send) bodyComfortTransmit(bus, BC_ID_UI_VEHICLE_CONTROL3, out, 8);
}

static void bodyComfortObserveVehicleStatus(uint8_t bus, const uint8_t *d, uint8_t dlc) {
  portENTER_CRITICAL(&bcMux);
  const bool enabled = bcCfg.sbEnabled;
  const uint8_t mask = bcCfg.sbSeatMask;
  portEXIT_CRITICAL(&bcMux);
  if (!enabled || mask == 0u) return;
  uint8_t out[8];
  const uint8_t result = bcSeatbeltTransformPure(d, dlc, mask, out);
  if (result == BC_SEATBELT_BAD_CHECKSUM) {
    portENTER_CRITICAL(&bcMux);
    bcState.seatbeltBadChecksum++;
    portEXIT_CRITICAL(&bcMux);
    return;
  }
  if (result != BC_SEATBELT_CHANGED) return;
  if (bodyComfortTransmit(bus, BC_ID_VCFRONT_VEHICLE_STATUS, out, 8)) {
    portENTER_CRITICAL(&bcMux);
    bcState.seatbeltInjected++;
    portEXIT_CRITICAL(&bcMux);
  }
}

// Entry point from the CAN RX task. `bus` is the physical bus the frame
// arrived on; frames from a bus that is not the active body bus are ignored.
static void bodyComfortObserve(uint8_t bus, uint16_t id, const uint8_t *d, uint8_t dlc) {
  if (d == nullptr || bus == BC_BUS_NONE || bus != bodyComfortActiveBus()) return;
  const uint32_t now = (uint32_t)millis();
  switch (id) {
    case BC_ID_VCLEFT_DOOR: bodyComfortObserveDoor(0u, d, dlc, now); break;
    case BC_ID_VCRIGHT_DOOR: bodyComfortObserveDoor(1u, d, dlc, now); break;
    case BC_ID_VCLEFT_WINDOW: bodyComfortObserveWindow(0u, d, dlc, now); break;
    case BC_ID_VCRIGHT_WINDOW: bodyComfortObserveWindow(1u, d, dlc, now); break;
    case BC_ID_DI_SYSTEM: {
      const uint8_t gear = bcDecodeGearPure(d, dlc);
      uint16_t pedal = 0;
      bool brake = false;
      const bool pedalOk = bcDecodeAccelPedalPure(d, dlc, pedal);
      const bool brakeOk = bcDecodeBrakePure(d, dlc, brake);
      portENTER_CRITICAL(&bcMux);
      if (gear != BC_GEAR_INVALID) { bcState.gear = gear; bcState.gearMs = now; bcState.gearValid = true; }
      if (pedalOk) { bcState.pedalPct10 = pedal; bcState.pedalMs = now; bcState.pedalValid = true; }
      if (brakeOk) { bcState.brakeOn = brake; bcState.brakeMs = now; bcState.brakeValid = true; }
      portEXIT_CRITICAL(&bcMux);
      break;
    }
    case BC_ID_DI_SPEED: {
      int32_t kph10 = 0;
      if (!bcDecodeSpeedPure(d, dlc, kph10)) break;
      portENTER_CRITICAL(&bcMux);
      bcState.kph10 = kph10; bcState.speedMs = now; bcState.speedValid = true;
      portEXIT_CRITICAL(&bcMux);
      break;
    }
    case BC_ID_APP_ENV: {
      bool rainy = false, snowy = false;
      if (!bcDecodeWeatherPure(d, dlc, rainy, snowy)) break;
      portENTER_CRITICAL(&bcMux);
      bcState.rainy = rainy; bcState.snowy = snowy;
      bcState.weatherMs = now; bcState.weatherValid = true;
      portEXIT_CRITICAL(&bcMux);
      break;
    }
    case BC_ID_UI_STATUS: {
      bool active = false;
      if (!bcDecodeCameraActivePure(d, dlc, active)) break;
      portENTER_CRITICAL(&bcMux);
      bcState.cameraActive = active; bcState.uiStatusMs = now; bcState.uiStatusValid = true;
      portEXIT_CRITICAL(&bcMux);
      break;
    }
    case BC_ID_VCFRONT_LIGHTING: {
      uint8_t hazard = 0; bool indicator = false;
      if (!bcDecodeLightingPure(d, dlc, hazard, indicator)) break;
      portENTER_CRITICAL(&bcMux);
      bcState.hazardReq = hazard; bcState.indicatorOn = indicator;
      bcState.lightingMs = now; bcState.lightingValid = true;
      portEXIT_CRITICAL(&bcMux);
      break;
    }
    case BC_ID_VCSEC_AUTH: {
      BcLockFramePure lock = {};
      if (!bcDecodeLockPure(d, dlc, lock)) break;
      portENTER_CRITICAL(&bcMux);
      bcState.lock = lock; bcState.lockMs = now; bcState.lockValid = true;
      portEXIT_CRITICAL(&bcMux);
      break;
    }
    case BC_ID_VCLEFT_SWITCH: bodyComfortObserveSwitch(bus, d, dlc, now); break;
    case BC_ID_UI_VEHICLE_CONTROL: bodyComfortObserveVehicleControl(bus, d, dlc, now); break;
    case BC_ID_UI_VEHICLE_CONTROL2: bodyComfortObserveVehicleControl2(bus, d, dlc, now); break;
    case BC_ID_UI_VEHICLE_CONTROL3: bodyComfortObserveVehicleControl3(bus, d, dlc, now); break;
    case BC_ID_VCFRONT_VEHICLE_STATUS: bodyComfortObserveVehicleStatus(bus, d, dlc); break;
    default: break;
  }
}

// ─── Service helpers (bcMux held unless stated) ──────────────────────────
static bool bcGearFreshLocked(uint32_t now, uint8_t &gear) {
  if (bcAgeFreshPure(now, bcState.gearMs, bcState.gearValid, BC_GEAR_FRESH_MS)) {
    gear = bcState.gear;
    return true;
  }
  return false;
}

static bool bcDoorFreshLocked(uint32_t now, uint8_t door) {
  const uint8_t side = (door == BC_DOOR_FL || door == BC_DOOR_RL) ? 0u : 1u;
  return bcAgeFreshPure(now, bcState.doorMs[side], bcState.doorValid[side], BC_DOOR_FRESH_MS);
}

static bool bcWindowFreshLocked(uint32_t now, uint8_t door) {
  const uint8_t side = (door == BC_DOOR_FL || door == BC_DOOR_RL) ? 0u : 1u;
  return bcAgeFreshPure(now, bcState.winM0Ms[side], bcState.winM0Valid[side], BC_WINDOW_FRESH_MS) &&
         bcAgeFreshPure(now, bcState.winM1Ms[side], bcState.winM1Valid[side], BC_WINDOW_FRESH_MS);
}

static bool bcHazardPressStartLocked(uint32_t now) {
  BcPressRuntime &p = bcState.hazardPress;
  if (p.active && (int32_t)(p.busyUntilMs - now) > 0) return false;
  p.active = true;
  p.pressUntilMs = now + BC_BUTTON_PRESS_MS;
  p.busyUntilMs = p.pressUntilMs + BC_BUTTON_RELEASE_MS;
  bcState.hazardPresses++;
  return true;
}

static bool bcCameraPressStartLocked(uint32_t now) {
  BcPressRuntime &p = bcState.cameraPress;
  if (p.active && (int32_t)(p.busyUntilMs - now) > 0) return false;
  p.active = true;
  p.pressUntilMs = now + BC_BUTTON_PRESS_MS;
  p.busyUntilMs = p.pressUntilMs + BC_BUTTON_RELEASE_MS;
  bcState.cameraPress.framesSent = 0;
  bcState.cameraLastPressMs = now;
  bcState.cameraPresses++;
  return true;
}

static void bcMirrorCommandLocked(uint32_t now, uint8_t request) {
  bcState.mirrorCmd.active = true;
  bcState.mirrorCmd.framesLeft = BC_ONE_SHOT_FRAMES;
  bcState.mirrorCmd.startMs = now;
  bcState.mirrorCmdRequest = request;
  if (request == BC_MIRROR_REQ_RETRACT) {
    bcState.mirrorFoldedByUs = true;
    bcState.mirrorFoldMs = now;
    bcState.unfoldTimer = {};
    bcState.cameraOpenedByUs = false;
    bcState.cameraAttempts = 0;
    bcState.closeCameraAfterUnfold = false;
  } else {
    bcState.closeCameraAfterUnfold = bcState.cameraOpenedByUs;
    bcState.mirrorFoldedByUs = false;
    bcState.unfoldTimer = {};
  }
}

// Returns a reason string when rejected, nullptr when started.
static const char *bcStartHazardBlinkLocked(uint32_t now) {
  if (!bcCfg.hzBlinkEnabled) return "hazard blink disabled";
  if (bcState.blinkActive) return "already running";
  if (!bcAgeFreshPure(now, bcState.lightingMs, bcState.lightingValid, BC_LIGHTING_FRESH_MS))
    return "lighting status unavailable";
  if (bcState.hazardReq != BC_HAZARD_REQ_NONE) return "hazards already on";
  bcState.blinkActive = true;
  bcState.blinkStartMs = now;
  bcState.blinkOnSeen = false;
  bcState.blinkOnMs = 0;
  bcState.blinkRuns++;
  return nullptr;
}

// Existing (pre-add_feats) features, toggled through the same setters the
// dashboard uses. May write NVS and take the CAN TX barrier: never call with
// bcMux held. Returns nullptr on success, otherwise a reason.
static const char *bodyComfortToggleExistingFeature(uint8_t action) {
  switch (action) {
    case BC_ACTION_TOGGLE_NAG_KILLER: {
      if (!activeProfileNagSupported()) return "Nag Killer unavailable for this profile";
      NagConfig nc;
      portENTER_CRITICAL(&nagCfgMux); nc = nagCfg; portEXIT_CRITICAL(&nagCfgMux);
      nagCfgApplyActiveProfilePolicy(nc, false);
      nc.enabled = !nc.enabled;
      nagCfgClampAll(nc);
      const NagScrollStoredSnapshot scroll = nagScrollRuntimeSnapshot();
      if (!tsl9InputQuiesceForConfig()) return "TSL9 input cleanup pending; retry";
      setCanTxAdministrativeHold(true);
      if (!nagCfgSaveCandidateWithScroll(nc, scroll)) {
        setCanTxAdministrativeHold(false);
        return "save failed";
      }
      nagScrollRuntimeRestore(scroll);
      nagTsl9RuntimeReset();
      nagCfgCommit(nc);
      tsl9InputHardResetAfterQuiesce(false);
      setCanTxAdministrativeHold(false);
      return nullptr;
    }
    case BC_ACTION_TOGGLE_ISA_SUPPRESSION: {
      const bool next = !isaSuppressionControlSnapshot();
      if (next && !activeProfileIsaSuppressionSupported()) return "ISA suppression unavailable";
      return isaSuppressionControlApply(next) ? nullptr : "save failed";
    }
    case BC_ACTION_TOGGLE_DRIVER_MONITORING: {
      const bool next = !driverMonitoringControlSnapshot();
      if (next && !activeProfileDmsNagSupported()) return "driver monitoring control unavailable";
      return driverMonitoringControlApply(next) ? nullptr : "save failed";
    }
    case BC_ACTION_TOGGLE_AUTO_BLINKER: {
      const bool next = !blinkAEnabled;
      if (next && !activeProfileAdvancedEapSupported()) return "Auto Blinker unavailable";
      return setAutoBlinkerEnabled(next, true) ? nullptr : "save failed";
    }
    case BC_ACTION_TOGGLE_TLSSC:
      return setTlsscEnabled(!tlsscEnabled, true) ? nullptr : "save failed";
    case BC_ACTION_TOGGLE_DOOR_CANCEL: {
      if (!activeProfileBodyControlsSupported() && !activeProfileUsesYlTopology())
        return "door cancel requires Body CAN";
      FeatureCfgSnapshot next = featureCfgRuntimeSnapshot();
      next.door = !next.door;
      if (!featureCfgPersistCandidate(next)) return "save failed";
      featureCfgRuntimeRestore(next);
      return nullptr;
    }
    case BC_ACTION_TOGGLE_VISION_CONTROL:
      if (!visionControlSupported()) return "Visual Speed Control unavailable";
      return visionControlApplySettings(!visionControlRequestDisabled(), visionControlBusSnapshot())
          ? nullptr : "save failed";
    case BC_ACTION_TOGGLE_CONTINUOUS_AP: {
      ContApConfig next = continuousApControlSnapshot();
      if (next.method == ContApMethod::Unset) return "choose a Continuous AP method first";
      next.enabled = !next.enabled;
      if (next.enabled && continuousApGestureRoutePure(activeVehicleProfile, activeVehicleTopology,
                                                       next.method) == ContApRoute::None)
        return "Continuous AP method unsupported";
      return continuousApControlApply(next) ? nullptr : "save failed";
    }
    case BC_ACTION_TOGGLE_LISTEN_ONLY:
      // Same transition the USB logger uses; waits for the CAN supervisor.
      return canUsbRequestMode(!canUsbPassive()) ? nullptr : "listen-only switch failed";
    case BC_ACTION_TOGGLE_BATTERY_PREHEAT: {
      const bool next = !preheatEnabled;
      if (next && !activeProfilePreheatSupported()) return "preheat unavailable";
      return preheatMenuSetEnabled(next) ? nullptr : "save failed";
    }
    default:
      return "no action";
  }
}

static const char *const BC_DEFERRED_EXISTING = "deferred";

static const char *bcRunActionLocked(uint32_t now, uint8_t action) {
  if (bcActionIsExistingFeaturePure(action)) return BC_DEFERRED_EXISTING;
  switch (action) {
    case BC_ACTION_RUN_HAZARD_BLINK:
      return bcStartHazardBlinkLocked(now);
    case BC_ACTION_MIRROR_FOLD_NOW: {
      const bool folded = bcState.mirrorFoldedByUs ||
          bcState.mirrorFold[0] == BC_MIRROR_FOLDED || bcState.mirrorFold[1] == BC_MIRROR_FOLDED;
      if (bcState.mirrorCmd.active) return "mirror command busy";
      bcMirrorCommandLocked(now, folded ? BC_MIRROR_REQ_PRESENT : BC_MIRROR_REQ_RETRACT);
      return nullptr;
    }
    case BC_ACTION_CAMERA:
      return bcCameraPressStartLocked(now) ? nullptr : "camera button busy";
    case BC_ACTION_OPEN_FRUNK: {
      uint8_t gear = BC_GEAR_INVALID;
      const bool speedFresh = bcAgeFreshPure(now, bcState.speedMs, bcState.speedValid, BC_SPEED_FRESH_MS);
      if (!bcGearFreshLocked(now, gear) || gear != BC_GEAR_P ||
          (speedFresh && (bcState.kph10 > 20 || bcState.kph10 < -20))) {
        bcState.frunkRefused++;
        return "frunk only opens in P";
      }
      if (bcState.frunkCmd.active) return "frunk command busy";
      bcState.frunkCmd.active = true;
      bcState.frunkCmd.framesLeft = BC_ONE_SHOT_FRAMES;
      bcState.frunkCmd.startMs = now;
      return nullptr;
    }
    default:
      if (bodyComfortApplyToggleActionPure(bcCfg, action)) {
        bcCfgDirty = true;
        bcCfgDirtyMs = now;
        return nullptr;
      }
      return "no action";
  }
}

// ─── Easy Entry ──────────────────────────────────────────────────────────
static void bcEntryServiceLocked(uint32_t now) {
  uint8_t gear = BC_GEAR_INVALID;
  const bool gearFresh = bcGearFreshLocked(now, gear);
  const bool weatherFresh =
      bcAgeFreshPure(now, bcState.weatherMs, bcState.weatherValid, BC_WEATHER_FRESH_MS);
  const bool switchFresh =
      bcAgeFreshPure(now, bcState.switchM0Ms, bcState.switchM0Valid, BC_SWITCH_STALE_MS);
  bool foldRequested = false;

  for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
    BcEntryDoorRuntime &e = bcState.entry[i];
    const bool doorFresh = bcDoorFreshLocked(now, i);
    const uint8_t door = doorFresh ? bcState.door[i] : (uint8_t)BC_DOOR_STATE_UNKNOWN;
    const bool winFresh = bcWindowFreshLocked(now, i);

    // Learn travel references and keep the settled pre-entry snapshot.
    if (winFresh) {
      bcWindowLearnPure(e.learn, bcState.winMotion[i], bcState.winPosState[i], bcState.winPos[i]);
      const bool settledInput = door == BC_DOOR_STATE_CLOSED &&
          bcState.winMotion[i] == BC_WINDOW_STOPPED &&
          bcState.winPosState[i] != BC_WINPOS_UNKNOWN && e.phase == BC_ENTRY_IDLE;
      if (!settledInput) {
        e.candSinceMs = now;
        e.candPosState = BC_WINPOS_UNKNOWN;
      } else if (e.candPosState != bcState.winPosState[i]) {
        e.candPosState = bcState.winPosState[i];
        e.candSinceMs = now;
      } else if ((uint32_t)(now - e.candSinceMs) >= BC_ENTRY_SNAPSHOT_SETTLE_MS) {
        e.snapValid = true;
        e.snapPosState = bcState.winPosState[i];
        e.snapPosRaw = bcState.winPos[i];
      }
    } else if (e.phase == BC_ENTRY_IDLE) {
      e.snapValid = false;
    }

    // The driver's window switch cancels any pending automatic move.
    if (switchFresh && bcState.winSwitch[i] &&
        (e.phase == BC_ENTRY_OPENED || e.phase == BC_ENTRY_RESTORE_PENDING) && !e.inFlight) {
      e.phase = BC_ENTRY_IDLE;
      e.snapValid = false;
      bcState.entryCancelledSwitch++;
    }

    const bool openedEdge = e.lastDoor == BC_DOOR_STATE_CLOSED && door == BC_DOOR_STATE_OPEN;
    const bool closedEdge = e.lastDoor == BC_DOOR_STATE_OPEN && door == BC_DOOR_STATE_CLOSED;
    if (door != BC_DOOR_STATE_UNKNOWN) e.lastDoor = door;

    if (openedEdge && e.phase == BC_ENTRY_IDLE && bcCfg.eeEnabled &&
        ((bcCfg.eeDoorMask >> i) & 1u) != 0u) {
      const bool weatherBad = weatherFresh && (bcState.rainy || bcState.snowy);
      const bool weatherBlocked = bcCfg.eeWeatherBlock && (weatherBad || (bcCfg.eeWeatherStrict && !weatherFresh));
      if (!gearFresh || gear != BC_GEAR_P) {
        bcState.entryBlockedGear++;
      } else if (weatherBlocked) {
        bcState.entryBlockedWeather++;
      } else if (e.snapValid) {
        uint8_t pct = 0;
        const bool pctValid = bcWindowPercentOpenPure(e.learn, e.snapPosRaw, pct);
        const BcRestorePlanPure plan = bcRestorePlanPure(e.snapPosState, pctValid, pct);
        if (plan.valid && e.snapPosState != BC_WINPOS_FULL) {
          e.plan = plan;
          e.phase = BC_ENTRY_OPEN_PENDING;
          e.attempts = 0;
          e.closeSeen = false;
          e.dueMs = now;
          bcState.entryOpens++;
          foldRequested = true;
        }
      }
    }

    if (closedEdge) {
      if (e.phase == BC_ENTRY_OPEN_PENDING) {
        if (e.inFlight) e.closeSeen = true;
        else e.phase = BC_ENTRY_IDLE;
      } else if (e.phase == BC_ENTRY_OPENED) {
        if (e.plan.needed) {
          e.phase = BC_ENTRY_RESTORE_PENDING;
          e.attempts = 0;
          e.dueMs = now + bcCfg.eeRestoreDelayMs;
        } else {
          e.phase = BC_ENTRY_IDLE;
        }
      }
    }
  }

  if (foldRequested && bcCfg.eeMirrorFold && !bcState.mirrorFoldedByUs &&
      !bcState.mirrorCmd.active &&
      bcState.mirrorFold[0] != BC_MIRROR_FOLDED && bcState.mirrorFold[1] != BC_MIRROR_FOLDED)
    bcMirrorCommandLocked(now, BC_MIRROR_REQ_RETRACT);
}

enum BcWinOutcomeRuntime : uint8_t {
  BC_WIN_OUTCOME_NONE = 0,
  BC_WIN_OUTCOME_SENT,
  BC_WIN_OUTCOME_NO_0x294,     // no stock frame and no template to build one from
};

// Returns true and fills `synth` when a window-command frame must be built
// from the 0x294 template (no stock 0x294 arrived after the command started).
static bool bcWindowCommandServiceLocked(uint32_t now, uint8_t synth[8]) {
  BcOneShotRuntime &w = bcState.winCmd;
  if (w.active) {
    const bool done = w.framesLeft == 0u;
    if (!done && (uint32_t)(now - w.startMs) <= BC_ONE_SHOT_TIMEOUT_MS) {
      const bool templateFresh = bcState.vc3Valid &&
          (uint32_t)(now - bcState.vc3Ms) <= BC_VC3_TEMPLATE_MAX_AGE_MS;
      if (!bcState.winCmdStockSeen && templateFresh &&
          (uint32_t)(now - w.startMs) >= BC_VC3_SYNTH_AFTER_MS &&
          (bcState.winCmdSynthSent == 0u ||
           (uint32_t)(now - bcState.winCmdLastSynthMs) >= BC_VC3_SYNTH_INTERVAL_MS)) {
        memcpy(synth, bcState.vc3Template, 8);
        bcClearVc3TransientPure(synth);
        if (bcComposeWindowRequestPure(synth, bcState.winCmdMask, bcState.winCmdRequest,
                                       bcState.winCmdPct)) {
          w.framesLeft--;
          bcState.winCmdSynthSent++;
          bcState.winCmdLastSynthMs = now;
          return true;
        }
      }
      return false;
    }
    // At least one delivered frame counts: stock 0x294 cadence is not guaranteed.
    const bool timedOut = w.framesLeft >= BC_WINDOW_CMD_FRAMES;
    if (timedOut) bcState.winCmdTimeouts++;
    bcState.lastWinRequest = bcState.winCmdRequest;
    bcState.lastWinMask = bcState.winCmdMask;
    bcState.lastWinStock = bcState.winCmdStockSent;
    bcState.lastWinSynth = bcState.winCmdSynthSent;
    bcState.lastWinOutcome = timedOut ? (uint8_t)BC_WIN_OUTCOME_NO_0x294 : (uint8_t)BC_WIN_OUTCOME_SENT;
    bcState.lastWinMs = now;
    for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
      BcEntryDoorRuntime &e = bcState.entry[i];
      if (!e.inFlight || ((bcState.winCmdMask >> i) & 1u) == 0u) continue;
      e.inFlight = false;
      if (e.phase == BC_ENTRY_OPEN_PENDING) {
        if (timedOut) { e.phase = BC_ENTRY_IDLE; continue; }
        if (e.closeSeen && e.plan.needed) {
          e.phase = BC_ENTRY_RESTORE_PENDING;
          e.attempts = 0;
          e.dueMs = now + bcCfg.eeRestoreDelayMs;
        } else {
          e.phase = e.closeSeen ? (uint8_t)BC_ENTRY_IDLE : (uint8_t)BC_ENTRY_OPENED;
        }
      } else if (e.phase == BC_ENTRY_RESTORE_PENDING) {
        if (!timedOut) {
          e.phase = BC_ENTRY_IDLE;
          e.snapValid = false;
          bcState.entryRestores++;
        } else if (++e.attempts >= 2u) {
          e.phase = BC_ENTRY_IDLE;
        } else {
          e.dueMs = now + 500u;
        }
      }
    }
    w = {};
    return false;
  }

  // Pick the first due door, then batch every due door sharing its command.
  int8_t lead = -1;
  for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
    const BcEntryDoorRuntime &e = bcState.entry[i];
    if ((e.phase == BC_ENTRY_OPEN_PENDING || e.phase == BC_ENTRY_RESTORE_PENDING) &&
        !e.inFlight && (int32_t)(now - e.dueMs) >= 0) { lead = (int8_t)i; break; }
  }
  if (lead < 0) return false;
  const uint8_t openReq = bcCfg.eeOpenPercent ? (uint8_t)BC_WINREQ_PERCENT : (uint8_t)BC_WINREQ_OPEN;
  const uint8_t openPct = bcCfg.eeOpenPercent ? 100u : 0u;
  const BcEntryDoorRuntime &l = bcState.entry[lead];
  const uint8_t request = l.phase == BC_ENTRY_OPEN_PENDING ? openReq : l.plan.request;
  const uint8_t pct = l.phase == BC_ENTRY_OPEN_PENDING ? openPct : l.plan.percent;
  uint8_t mask = 0;
  for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
    BcEntryDoorRuntime &e = bcState.entry[i];
    if (e.inFlight || (int32_t)(now - e.dueMs) < 0) continue;
    const bool open = e.phase == BC_ENTRY_OPEN_PENDING;
    const bool restore = e.phase == BC_ENTRY_RESTORE_PENDING;
    if (!open && !restore) continue;
    const uint8_t r = open ? openReq : e.plan.request;
    const uint8_t p = open ? openPct : e.plan.percent;
    if (r != request || p != pct) continue;
    e.inFlight = true;
    mask |= (uint8_t)(1u << i);
  }
  if (mask == 0u) return false;
  bcState.winCmdMask = mask;
  bcState.winCmdRequest = request;
  bcState.winCmdPct = pct;
  bcState.winCmdStockSent = 0;
  bcState.winCmdSynthSent = 0;
  bcState.winCmdStockSeen = false;
  w.active = true;
  w.framesLeft = BC_WINDOW_CMD_FRAMES;
  w.startMs = now;
  return false;
}

// ─── Mirrors / camera ────────────────────────────────────────────────────
static void bcMirrorServiceLocked(uint32_t now) {
  BcOneShotRuntime &m = bcState.mirrorCmd;
  if (m.active && (m.framesLeft == 0u || (uint32_t)(now - m.startMs) > BC_ONE_SHOT_TIMEOUT_MS)) {
    const bool sent = m.framesLeft < BC_ONE_SHOT_FRAMES;
    m = {};
    const bool retract = bcState.mirrorCmdRequest == BC_MIRROR_REQ_RETRACT;
    if (sent) {
      if (retract) bcState.mirrorFolds++;
      else bcState.mirrorUnfolds++;
    } else {
      bcState.mirrorCmdTimeouts++;
      if (retract) bcState.mirrorFoldedByUs = false;
    }
  }
  uint8_t gear = BC_GEAR_INVALID;
  const bool gearFresh = bcGearFreshLocked(now, gear);
  const bool doorsFresh =
      bcAgeFreshPure(now, bcState.doorMs[0], bcState.doorValid[0], BC_DOOR_FRESH_MS) &&
      bcAgeFreshPure(now, bcState.doorMs[1], bcState.doorValid[1], BC_DOOR_FRESH_MS);
  const bool cameraFresh =
      bcAgeFreshPure(now, bcState.uiStatusMs, bcState.uiStatusValid, BC_UI_STATUS_FRESH_MS);

  if (bcState.mirrorFoldedByUs && !m.active) {
    // Driver unfolded manually: we no longer own the fold.
    if (doorsFresh && bcState.mirrorFold[0] == BC_MIRROR_UNFOLDED &&
        bcState.mirrorFold[1] == BC_MIRROR_UNFOLDED &&
        (uint32_t)(now - bcState.mirrorFoldMs) > BC_MIRROR_USER_UNFOLD_GRACE_MS) {
      bcState.mirrorFoldedByUs = false;
      bcState.closeCameraAfterUnfold = bcState.cameraOpenedByUs;
      bcState.unfoldTimer = {};
    } else {
      const bool speedFresh =
          bcAgeFreshPure(now, bcState.speedMs, bcState.speedValid, BC_SPEED_FRESH_MS);
      if (bcUnfoldDuePure(bcState.unfoldTimer, now, gearFresh && gear == BC_GEAR_D, speedFresh,
                          bcState.kph10, bcCfg.eeUnfoldKph, bcCfg.eeUnfoldHoldS)) {
        bcMirrorCommandLocked(now, BC_MIRROR_REQ_PRESENT);
        return;
      }
      // Camera view while the mirrors are folded by this feature.
      if (bcCfg.eeMirrorCamera && cameraFresh && gearFresh &&
          (gear == BC_GEAR_D || gear == BC_GEAR_N)) {
        if (bcState.cameraActive) {
          if ((uint32_t)(now - bcState.cameraLastPressMs) < 5000u && bcState.cameraAttempts > 0u)
            bcState.cameraOpenedByUs = true;
        } else if (!bcState.cameraOpenedByUs && bcState.cameraAttempts < BC_CAMERA_MAX_ATTEMPTS &&
                   (bcState.cameraAttempts == 0u ||
                    (uint32_t)(now - bcState.cameraLastPressMs) >= BC_CAMERA_RETRY_MS)) {
          if (bcCameraPressStartLocked(now)) bcState.cameraAttempts++;
        }
      }
    }
  }
  // After unfolding, close the camera view this feature opened.
  if (bcState.closeCameraAfterUnfold && !m.active && cameraFresh) {
    if (bcState.cameraActive) (void)bcCameraPressStartLocked(now);
    bcState.closeCameraAfterUnfold = false;
    bcState.cameraOpenedByUs = false;
  }
}

// ─── Hazards / rear light ────────────────────────────────────────────────
static void bcHazardServiceLocked(uint32_t now) {
  uint8_t gear = BC_GEAR_INVALID;
  const bool gearFresh = bcGearFreshLocked(now, gear);
  const bool lightingFresh =
      bcAgeFreshPure(now, bcState.lightingMs, bcState.lightingValid, BC_LIGHTING_FRESH_MS);

  bcState.reverseHazardWanted = bcCfg.rvHazard && gearFresh && gear == BC_GEAR_R;
  bcState.rearLightActive = bcCfg.rvLight && gearFresh && gear == BC_GEAR_R;

  bool doorsValid = true, anyOpen = false;
  for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
    if (!bcDoorFreshLocked(now, i) || bcState.door[i] == BC_DOOR_STATE_UNKNOWN) doorsValid = false;
    else if (bcState.door[i] == BC_DOOR_STATE_OPEN) anyOpen = true;
  }
  bcState.doorHazardWanted = bcDoorHazardStepPure(
      bcState.doorHazard, now, bcCfg.drHazard,
      gearFresh && (gear == BC_GEAR_D || gear == BC_GEAR_R), doorsValid || anyOpen, anyOpen,
      bcCfg.drOffDelayMs);

  // Hazard blink: N seconds counted from the moment the hazards are seen on.
  if (bcState.blinkActive) {
    const uint32_t durationMs = (uint32_t)bcCfg.hzBlinkSeconds * 1000u;
    if (!bcState.blinkOnSeen && lightingFresh && bcState.hazardReq == BC_HAZARD_REQ_BUTTON) {
      bcState.blinkOnSeen = true;
      bcState.blinkOnMs = now;
    }
    const bool elapsed = bcState.blinkOnSeen && (uint32_t)(now - bcState.blinkOnMs) >= durationMs;
    const bool neverOn = (uint32_t)(now - bcState.blinkStartMs) > durationMs + 4000u;
    if (elapsed || neverOn || !bcCfg.hzBlinkEnabled) bcState.blinkActive = false;
  }

  const bool desired = bcState.reverseHazardWanted || bcState.doorHazardWanted || bcState.blinkActive;
  if (bcHazardStepPure(bcState.hazardCtl, now, lightingFresh, bcState.hazardReq, desired) ==
      BC_HAZARD_ACTION_PRESS) {
    if (!bcHazardPressStartLocked(now)) {
      // Injector still busy: let the controller retry after its settle window.
      bcState.hazardCtl.pressPending = false;
      if (bcState.hazardCtl.attempts > 0u) bcState.hazardCtl.attempts--;
    }
  }
  if (bcState.hazardPress.active && (int32_t)(bcState.hazardPress.busyUntilMs - now) <= 0)
    bcState.hazardPress.active = false;
  if (bcState.cameraPress.active && (int32_t)(bcState.cameraPress.busyUntilMs - now) <= 0)
    bcState.cameraPress.active = false;
}

static void bcMacroServiceLocked(uint32_t now, uint8_t *actions, uint8_t &actionCount) {
  actionCount = 0;
  for (uint8_t b = 0; b < BC_BTN_COUNT; ++b) {
    const uint8_t ev = bcGestureTickPure(bcState.gesture[b], now, bcCfg.mcGapMs, BC_SWITCH_STALE_MS);
    if (ev == BC_GESTURE_NONE || !bcCfg.mcEnabled) continue;
    if (bcState.macroCount >= BC_MACRO_QUEUE_LEN) { bcState.macroDropped++; continue; }
    const uint8_t slot = (uint8_t)((bcState.macroHead + bcState.macroCount) % BC_MACRO_QUEUE_LEN);
    bcState.macroQueue[slot] = {b, ev};
    bcState.macroCount++;
  }
  while (bcState.macroCount > 0u && actionCount < BC_MACRO_QUEUE_LEN) {
    const BcMacroEventRuntime ev = bcState.macroQueue[bcState.macroHead];
    bcState.macroHead = (uint8_t)((bcState.macroHead + 1u) % BC_MACRO_QUEUE_LEN);
    bcState.macroCount--;
    if (!bcCfg.mcEnabled || ev.button >= BC_BTN_COUNT || ev.gesture >= BC_GESTURE_COUNT) continue;
    const uint8_t action = bcCfg.mcAction[ev.button][ev.gesture];
    bcState.macroEvents++;
    bcState.lastMacroButton = ev.button;
    bcState.lastMacroGesture = ev.gesture;
    bcState.lastMacroAction = action;
    bcState.lastMacroMs = now;
    if (action != BC_ACTION_NONE) actions[actionCount++] = action;
  }
}

// Door-handle macro: holding an enabled exterior handle for dhHoldS runs
// dhAction once per pull. The hold time counts only while the car and that
// door are unlocked and the door is open (fresh 0x339 lock state, fail-closed).
// Reported as buttons 8 (driver) / 9 (passenger).
static constexpr uint8_t BC_HANDLE_BUTTON_BASE = 8u;

static void bcHandleServiceLocked(uint32_t now, uint8_t *actions, uint8_t &actionCount) {
  const bool lockFresh = bcAgeFreshPure(now, bcState.lockMs, bcState.lockValid, BC_LOCK_FRESH_MS);
  for (uint8_t side = 0; side < 2u; ++side) {
    const bool fresh = bcAgeFreshPure(now, bcState.doorMs[side], bcState.doorValid[side], BC_DOOR_FRESH_MS);
    const bool armed = bcCfg.dhEnabled && ((bcCfg.dhDoorMask >> side) & 1u) != 0u;
    if (!armed || !fresh || !bcState.handlePulled[side]) {
      bcState.handleTiming[side] = false;
      bcState.handleFired[side] = false;
      bcState.handleBlockNoted[side] = false;
      continue;
    }
    const uint8_t frontDoor = bcState.door[side == 0u ? BC_DOOR_FL : BC_DOOR_FR];
    if (!bcHandleMacroAllowedPure(lockFresh, bcState.lock, side, frontDoor)) {
      // Still pulled, but locked / no lock data / door not open: wait.
      if (!bcState.handleFired[side] && !bcState.handleBlockNoted[side]) {
        bcState.handleBlockNoted[side] = true;
        if (!lockFresh || bcState.lock.simple != 1u || bcState.lock.frontLocked[side])
          bcState.handleBlockedLocked++;
        else
          bcState.handleBlockedClosed++;
      }
      bcState.handleTiming[side] = false;
      continue;
    }
    if (!bcState.handleTiming[side]) {
      bcState.handleTiming[side] = true;
      bcState.handleSinceMs[side] = now;
    }
    if (bcState.handleFired[side] ||
        (uint32_t)(now - bcState.handleSinceMs[side]) < (uint32_t)bcCfg.dhHoldS * 1000u)
      continue;
    bcState.handleFired[side] = true;
    bcState.macroEvents++;
    bcState.lastMacroButton = (uint8_t)(BC_HANDLE_BUTTON_BASE + side);
    bcState.lastMacroGesture = BC_GESTURE_LONG;
    bcState.lastMacroAction = bcCfg.dhAction;
    bcState.lastMacroMs = now;
    if (bcCfg.dhAction != BC_ACTION_NONE && actionCount < BC_MACRO_QUEUE_LEN)
      actions[actionCount++] = bcCfg.dhAction;
  }
}

static uint8_t bcKickdownServiceLocked(uint32_t now, bool &gearDOut) {
  uint8_t gear = BC_GEAR_INVALID;
  BcKickdownInputPure in = {};
  in.enabled = bcCfg.kdEnabled && activeProfilePedalMapSupported();
  in.gearD = bcGearFreshLocked(now, gear) && gear == BC_GEAR_D;
  in.pedalValid = bcAgeFreshPure(now, bcState.pedalMs, bcState.pedalValid, BC_GEAR_FRESH_MS);
  in.pedalPct10 = bcState.pedalPct10;
  in.brakeValid = bcAgeFreshPure(now, bcState.brakeMs, bcState.brakeValid, BC_GEAR_FRESH_MS);
  in.brakeOn = bcState.brakeOn;
  gearDOut = in.gearD;
  return bcKickdownStepPure(bcState.kickdown, now, in, bcCfg);
}

static void bcOneShotDoneLocked(uint32_t now, BcOneShotRuntime &cmd, uint32_t &ok, uint32_t &timeouts) {
  if (!cmd.active) return;
  if (cmd.framesLeft != 0u && (uint32_t)(now - cmd.startMs) <= BC_ONE_SHOT_TIMEOUT_MS) return;
  if (cmd.framesLeft < BC_ONE_SHOT_FRAMES) ok++;
  else timeouts++;
  cmd = {};
}

static void bodyComfortServiceTick(uint32_t now) {
  if (bodyComfortActiveBus() == BC_BUS_NONE) return;
  uint8_t actions[BC_MACRO_QUEUE_LEN + 2u];
  uint8_t actionCount = 0;
  bool save = false;
  BodyComfortConfigPure toSave = {};
  uint8_t existing[BC_MACRO_QUEUE_LEN + 2u];
  uint8_t existingCount = 0;
  portENTER_CRITICAL(&bcMux);
  bcMacroServiceLocked(now, actions, actionCount);
  bcHandleServiceLocked(now, actions, actionCount);
  for (uint8_t i = 0; i < actionCount; ++i) {
    const char *r = bcRunActionLocked(now, actions[i]);
    if (r == BC_DEFERRED_EXISTING) existing[existingCount++] = actions[i];
    else bcState.lastMacroResult = r ? r : "OK";
  }
  bcEntryServiceLocked(now);
  uint8_t synth[8];
  const bool synthSend = bcWindowCommandServiceLocked(now, synth);
  uint8_t fogFrame[8], lightFrame[8];
  bool fogSend = false, lightSend = false;
  bcMirrorServiceLocked(now);
  bcHazardServiceLocked(now);
  bcRearLightReinforceLocked(now, fogFrame, fogSend, lightFrame, lightSend);
  bcOneShotDoneLocked(now, bcState.frunkCmd, bcState.frunkOpens, bcState.frunkRefused);
  bool kdGearD = false;
  const uint8_t kdAction = bcKickdownServiceLocked(now, kdGearD);
  const uint8_t kdTarget = bcCfg.kdTargetRaw;
  const uint8_t kdRelease = bcCfg.kdReleaseMode == BC_KD_RELEASE_STOCK
      ? PEDAL_MAP_RAW_STOCK : PEDAL_MAP_RAW_CHILL;
  if (bcCfgDirty && (uint32_t)(now - bcCfgDirtyMs) >= BC_CFG_SAVE_DEBOUNCE_MS) {
    bcCfgDirty = false;
    toSave = bcCfg;
    save = true;
  }
  portEXIT_CRITICAL(&bcMux);
  if (synthSend) bodyComfortTransmit(bodyComfortActiveBus(), BC_ID_UI_VEHICLE_CONTROL3, synth, 8);
  if (fogSend && !bodyComfortTransmit(bodyComfortActiveBus(), BC_ID_UI_VEHICLE_CONTROL, fogFrame, 8)) {
    portENTER_CRITICAL(&bcMux); bcState.rearLightTxFail++; bcState.fogLastTxMs = now - BC_OVERRIDE_REINFORCE_MS; portEXIT_CRITICAL(&bcMux);
  }
  if (lightSend && !bodyComfortTransmit(bodyComfortActiveBus(), BC_ID_UI_VEHICLE_CONTROL2, lightFrame, 8)) {
    portENTER_CRITICAL(&bcMux); bcState.rearLightTxFail++; bcState.lightLastTxMs = now - BC_OVERRIDE_REINFORCE_MS; portEXIT_CRITICAL(&bcMux);
  }
  for (uint8_t i = 0; i < existingCount; ++i) {
    const char *r = bodyComfortToggleExistingFeature(existing[i]);
    portENTER_CRITICAL(&bcMux);
    bcState.lastMacroResult = r ? r : "OK";
    portEXIT_CRITICAL(&bcMux);
  }
  if (kdAction != BC_KD_ACTION_NONE) {
    // Existing Pedal Response path: same AP / Summon / 0x334 freshness gates.
    const bool ok = requestPedalMapMode(
        kdAction == BC_KD_ACTION_ENGAGE ? kdTarget : kdRelease,
        kdAction == BC_KD_ACTION_ENGAGE ? "Kickdown" : "Kickdown release");
    portENTER_CRITICAL(&bcMux);
    if (!ok) {
      bcKickdownRequestFailedPure(bcState.kickdown, now, kdAction, kdGearD);
      bcState.kickdownRefused++;
    } else if (kdAction == BC_KD_ACTION_ENGAGE) {
      bcState.kickdownEngages++;
    } else {
      bcState.kickdownReleases++;
    }
    portEXIT_CRITICAL(&bcMux);
  }
  if (save && !bodyComfortCfgPersist(toSave)) {
    portENTER_CRITICAL(&bcMux);
    bcCfgSaveFailures++;
    portEXIT_CRITICAL(&bcMux);
  }
}

static void bodyComfortTask(void *) {
  for (;;) {
    bodyComfortServiceTick((uint32_t)millis());
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static void bodyComfortStart() {
  if (bodyComfortActiveBus() == BC_BUS_NONE) {
    T2CAN_SERIAL_PRINTLN("Body Comfort: unsupported profile/topology · task not started");
    return;
  }
  const BaseType_t ret = xTaskCreatePinnedToCore(bodyComfortTask, "bodyCmf", 5120, nullptr, 2,
                                                 &bodyComfortTaskHandle, 0);
  if (ret != pdPASS) {
    bodyComfortTaskHandle = nullptr;
    T2CAN_SERIAL_PRINTF("Body Comfort task creation failed: %d\n", ret);
  }
}

// ─── HTTP API ────────────────────────────────────────────────────────────
static const char *bcGearName(uint8_t gear) {
  switch (gear) {
    case BC_GEAR_P: return "P";
    case BC_GEAR_R: return "R";
    case BC_GEAR_N: return "N";
    case BC_GEAR_D: return "D";
    default: return "?";
  }
}

static const char *bcDoorName(uint8_t state) {
  if (state == BC_DOOR_STATE_OPEN) return "OPEN";
  if (state == BC_DOOR_STATE_CLOSED) return "CLOSED";
  return "?";
}

static const char *bcWinPosName(uint8_t s) {
  static const char *const names[] = {"?", "CLOSED", "CRACKED", "VENT", "PARTIAL", "OPEN", "CLOSED"};
  return s < 7u ? names[s] : "?";
}

static const char *bcMirrorName(uint8_t s) {
  static const char *const names[] = {"?", "FOLDED", "UNFOLDED", "FOLDING", "UNFOLDING"};
  return s < 5u ? names[s] : "?";
}

static const char *bcEntryPhaseName(uint8_t p) {
  static const char *const names[] = {"IDLE", "OPENING", "OPENED", "RESTORING"};
  return p < 4u ? names[p] : "?";
}

static String bodyComfortJson() {
  const uint32_t now = (uint32_t)millis();
  const uint8_t bus = bodyComfortActiveBus();
  portENTER_CRITICAL(&bcMux);
  const BodyComfortConfigPure c = bcCfg;
  const BcStateRuntime s = bcState;
  const uint32_t saveFailures = bcCfgSaveFailures;
  portEXIT_CRITICAL(&bcMux);

  String j;
  j.reserve(2600);
  JsonWriterArduino jw(j);
  jw.boolean("ok", true);
  jw.boolean("supported", bus != BC_BUS_NONE);
  jw.string("bus", bodyComfortBusName(bus));
  jw.string("layout", (activeVehicleProfile == VEHICLE_MODEL_Y_JUNIPER || activeProfileIsYl())
                          ? "JUNIPER_VEH" : (bus == BC_BUS_NONE ? "NONE" : "EXPERIMENTAL"));
  jw.boolean("taskRunning", bodyComfortTaskHandle != nullptr);

  jw.beginObject("cfg");
  jw.boolean("eeEnabled", c.eeEnabled);
  jw.u32("eeDoorMask", c.eeDoorMask);
  jw.boolean("eeWeatherBlock", c.eeWeatherBlock);
  jw.boolean("eeWeatherStrict", c.eeWeatherStrict);
  jw.u32("eeRestoreDelayMs", c.eeRestoreDelayMs);
  jw.boolean("eeOpenPercent", c.eeOpenPercent);
  jw.boolean("eeMirrorFold", c.eeMirrorFold);
  jw.boolean("eeMirrorCamera", c.eeMirrorCamera);
  jw.u32("eeUnfoldKph", c.eeUnfoldKph);
  jw.u32("eeUnfoldHoldS", c.eeUnfoldHoldS);
  jw.boolean("sbEnabled", c.sbEnabled);
  jw.u32("sbSeatMask", c.sbSeatMask);
  jw.boolean("mcEnabled", c.mcEnabled);
  jw.u32("mcLongMs", c.mcLongMs);
  jw.u32("mcGapMs", c.mcGapMs);
  String macro = "[";
  for (uint8_t b = 0; b < BC_BTN_COUNT; ++b) {
    if (b) macro += ',';
    macro += '[';
    for (uint8_t g = 0; g < BC_GESTURE_COUNT; ++g) {
      if (g) macro += ',';
      macro += (unsigned)c.mcAction[b][g];
    }
    macro += ']';
  }
  macro += ']';
  jw.raw("mcAction", macro);
  jw.boolean("hzBlinkEnabled", c.hzBlinkEnabled);
  jw.u32("hzBlinkSeconds", c.hzBlinkSeconds);
  jw.boolean("rvHazard", c.rvHazard);
  jw.boolean("rvLight", c.rvLight);
  jw.u32("rvLightMode", c.rvLightMode);
  jw.boolean("drHazard", c.drHazard);
  jw.u32("drOffDelayMs", c.drOffDelayMs);
  jw.boolean("kdEnabled", c.kdEnabled);
  jw.u32("kdTriggerPct", c.kdTriggerPct);
  jw.u32("kdTargetRaw", c.kdTargetRaw);
  jw.boolean("kdReleaseOnPedal", c.kdReleaseOnPedal);
  jw.u32("kdReleasePct", c.kdReleasePct);
  jw.boolean("kdReleaseOnBrake", c.kdReleaseOnBrake);
  jw.u32("kdReleaseMode", c.kdReleaseMode);
  jw.boolean("dhEnabled", c.dhEnabled);
  jw.u32("dhDoorMask", c.dhDoorMask);
  jw.u32("dhHoldS", c.dhHoldS);
  jw.u32("dhAction", c.dhAction);
  jw.endObject();

  const bool gearFresh = bcAgeFreshPure(now, s.gearMs, s.gearValid, BC_GEAR_FRESH_MS);
  const bool speedFresh = bcAgeFreshPure(now, s.speedMs, s.speedValid, BC_SPEED_FRESH_MS);
  const bool weatherFresh = bcAgeFreshPure(now, s.weatherMs, s.weatherValid, BC_WEATHER_FRESH_MS);
  const bool lightingFresh = bcAgeFreshPure(now, s.lightingMs, s.lightingValid, BC_LIGHTING_FRESH_MS);
  const bool cameraFresh = bcAgeFreshPure(now, s.uiStatusMs, s.uiStatusValid, BC_UI_STATUS_FRESH_MS);
  jw.beginObject("live");
  jw.string("gear", gearFresh ? bcGearName(s.gear) : "?");
  if (speedFresh) jw.fixed("speedKph", s.kph10, 1);
  else jw.raw("speedKph", "null");
  jw.string("weather", !weatherFresh ? "UNKNOWN"
                       : (s.rainy && s.snowy) ? "RAIN+SNOW"
                       : s.rainy ? "RAIN" : s.snowy ? "SNOW" : "CLEAR");
  String doors;
  String windows;
  String entry;
  static const char *const doorKeys[BC_DOOR_COUNT] = {"FL", "FR", "RL", "RR"};
  for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
    const uint8_t side = (i == BC_DOOR_FL || i == BC_DOOR_RL) ? 0u : 1u;
    const bool dFresh = bcAgeFreshPure(now, s.doorMs[side], s.doorValid[side], BC_DOOR_FRESH_MS);
    const bool wFresh = bcAgeFreshPure(now, s.winM1Ms[side], s.winM1Valid[side], BC_WINDOW_FRESH_MS);
    if (i) { doors += ' '; windows += ' '; entry += ' '; }
    doors += doorKeys[i]; doors += ':'; doors += dFresh ? bcDoorName(s.door[i]) : "?";
    windows += doorKeys[i]; windows += ':'; windows += wFresh ? bcWinPosName(s.winPosState[i]) : "?";
    uint8_t pct = 0;
    if (wFresh && bcWindowPercentOpenPure(s.entry[i].learn, s.winPos[i], pct)) {
      windows += '('; windows += (unsigned)pct; windows += "%)";
    }
    entry += doorKeys[i]; entry += ':'; entry += bcEntryPhaseName(s.entry[i].phase);
  }
  jw.string("doors", doors);
  jw.string("windows", windows);
  jw.string("easyEntry", entry);
  {
    static const char *const unavailNames[] = {"NONE", "UNEXPECTED", "SWITCH", "PINCH", "THERMAL",
                                               "UNCALIBRATED", "IN_DRIVE", "MOVING"};
    static const char *const stopNames[] = {"NONE", "DOOR_OPEN_TRIM", "NEW_REQ", "TARGET", "STOP_REQ",
                                            "FULL_OPEN", "TIMEOUT", "?"};
    static const char *const reqNames[] = {"IDLE", "PERCENT", "VENT", "CLOSED", "OPEN"};
    String diag;
    for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
      const uint8_t side = (i == BC_DOOR_FL || i == BC_DOOR_RL) ? 0u : 1u;
      const bool wFresh = bcAgeFreshPure(now, s.winM1Ms[side], s.winM1Valid[side], BC_WINDOW_FRESH_MS);
      if (i) diag += ' ';
      diag += doorKeys[i]; diag += ':';
      if (!wFresh) { diag += '?'; continue; }
      diag += unavailNames[s.winUnavail[i] & 7u]; diag += '/'; diag += stopNames[s.winStop[i] & 7u];
    }
    jw.string("windowsDiag", diag);
    String vc3;
    if (s.vc3Valid) {
      const unsigned long tenths = (unsigned long)((now - s.vc3Ms) / 100u);
      vc3 += tenths / 10u; vc3 += '.'; vc3 += tenths % 10u; vc3 += " s ago · ";
      vc3 += (unsigned long)s.vc3RxCount; vc3 += " rx";
    } else {
      vc3 = "never seen";
    }
    jw.string("vc3", vc3);
    String cmd = "—";
    if (s.lastWinOutcome != BC_WIN_OUTCOME_NONE) {
      cmd = String(reqNames[s.lastWinRequest <= 4u ? s.lastWinRequest : 0u]) + " ";
      for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i)
        if ((s.lastWinMask >> i) & 1u) { cmd += doorKeys[i]; cmd += ' '; }
      cmd += "· stock "; cmd += (unsigned)s.lastWinStock;
      cmd += " + synth "; cmd += (unsigned)s.lastWinSynth;
      cmd += s.lastWinOutcome == BC_WIN_OUTCOME_SENT ? " · SENT" : " · NO 0x294";
      cmd += " · "; cmd += (unsigned long)((now - s.lastWinMs) / 1000u); cmd += " s ago";
    }
    jw.string("windowCmd", cmd);
    // Touchscreen window requests seen on the bus, newest first.
    String seen;
    for (uint8_t k = 0; k < s.vc3SeenCount; ++k) {
      const uint8_t idx = (uint8_t)((s.vc3SeenHead + 4u - 1u - k) % 4u);
      const uint8_t *f = s.vc3Seen[idx];
      if (k) seen += " | ";
      seen += reqNames[bcGetBitsPure(f, 52, 3) <= 4u ? bcGetBitsPure(f, 52, 3) : 0u];
      seen += ' ';
      for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i)
        if (bcGetBitsPure(f, bcWindowRequestedBitPure(i), 1)) { seen += doorKeys[i]; seen += ' '; }
      const int32_t raw6 = (int32_t)bcGetBitsPure(f, 42, 6);
      seen += "pct "; seen += (long)(((raw6 & 0x20) ? raw6 - 64 : raw6) * 5);
      seen += " · ";
      char hex[24];
      snprintf(hex, sizeof(hex), "%02X%02X%02X%02X%02X%02X%02X%02X", f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
      seen += hex;
      seen += " · "; seen += (unsigned long)((now - s.vc3SeenMs[idx]) / 1000u); seen += " s ago";
    }
    jw.string("vc3Seen", seen.length() ? seen : String("none yet (press Vent / Close on the touchscreen)"));
    jw.u32("vc3WindowFrames", s.vc3WindowFrames);
  }
  jw.string("mirrors", String("L:") + bcMirrorName(s.mirrorFold[0]) + " R:" + bcMirrorName(s.mirrorFold[1]));
  jw.boolean("mirrorFoldedByFeature", s.mirrorFoldedByUs);
  jw.string("camera", cameraFresh ? (s.cameraActive ? "ON" : "OFF") : "?");
  jw.string("hazard", !lightingFresh ? "?" : s.hazardReq == 0u ? "OFF"
                      : s.hazardReq == BC_HAZARD_REQ_BUTTON ? "ON" : "OTHER");
  jw.boolean("hazardOwned", s.hazardCtl.ownedOn);
  jw.boolean("blinkActive", s.blinkActive);
  jw.boolean("reverseHazard", s.reverseHazardWanted);
  jw.boolean("doorHazard", s.doorHazardWanted);
  jw.boolean("rearLight", s.rearLightActive);
  const bool pedalFresh = bcAgeFreshPure(now, s.pedalMs, s.pedalValid, BC_GEAR_FRESH_MS);
  const bool brakeFresh = bcAgeFreshPure(now, s.brakeMs, s.brakeValid, BC_GEAR_FRESH_MS);
  if (pedalFresh) jw.fixed("pedalPct", s.pedalPct10, 1);
  else jw.raw("pedalPct", "null");
  jw.string("brake", brakeFresh ? (s.brakeOn ? "ON" : "OFF") : "?");
  jw.string("kickdown", !c.kdEnabled ? "OFF" : !activeProfilePedalMapSupported() ? "UNSUPPORTED"
                        : s.kickdown.kicked ? "KICKED" : "ARMED");
  const bool lFresh = bcAgeFreshPure(now, s.doorMs[0], s.doorValid[0], BC_DOOR_FRESH_MS);
  const bool rFresh = bcAgeFreshPure(now, s.doorMs[1], s.doorValid[1], BC_DOOR_FRESH_MS);
  const bool lockFresh = bcAgeFreshPure(now, s.lockMs, s.lockValid, BC_LOCK_FRESH_MS);
  jw.string("handles", String("DRIVER:") + (!lFresh ? "?" : s.handlePulled[0] ? "PULLED" : "-") +
                       " PASSENGER:" + (!rFresh ? "?" : s.handlePulled[1] ? "PULLED" : "-"));
  jw.string("lock", !lockFresh ? "?" : s.lock.simple == 1u ? "UNLOCKED" : "LOCKED");
  jw.boolean("listenOnly", canUsbPassive());
  jw.endObject();

  jw.beginObject("stats");
  jw.u32("txOk", s.txOk);
  jw.u32("txFail", s.txFail);
  jw.u32("entryOpens", s.entryOpens);
  jw.u32("entryRestores", s.entryRestores);
  jw.u32("entryBlockedWeather", s.entryBlockedWeather);
  jw.u32("entryBlockedGear", s.entryBlockedGear);
  jw.u32("entryCancelledSwitch", s.entryCancelledSwitch);
  jw.u32("windowTimeouts", s.winCmdTimeouts);
  jw.u32("mirrorFolds", s.mirrorFolds);
  jw.u32("mirrorUnfolds", s.mirrorUnfolds);
  jw.u32("mirrorTimeouts", s.mirrorCmdTimeouts);
  jw.u32("cameraPresses", s.cameraPresses);
  jw.u32("hazardPresses", s.hazardPresses);
  jw.u32("blinkRuns", s.blinkRuns);
  jw.u32("seatbeltInjected", s.seatbeltInjected);
  jw.u32("seatbeltBadChecksum", s.seatbeltBadChecksum);
  jw.u32("rearLightInjected", s.rearLightInjected);
  jw.u32("rearLightReinforced", s.rearLightReinforced);
  jw.u32("rearLightTxFail", s.rearLightTxFail);
  jw.u32("kickdownEngages", s.kickdownEngages);
  jw.u32("kickdownReleases", s.kickdownReleases);
  jw.u32("kickdownRefused", s.kickdownRefused);
  jw.u32("frunkOpens", s.frunkOpens);
  jw.u32("handleBlockedLocked", s.handleBlockedLocked);
  jw.u32("handleBlockedClosed", s.handleBlockedClosed);
  jw.u32("frunkRefused", s.frunkRefused);
  jw.u32("macroEvents", s.macroEvents);
  jw.u32("macroDropped", s.macroDropped);
  jw.u32("lastMacroButton", s.lastMacroButton);
  jw.u32("lastMacroGesture", s.lastMacroGesture);
  jw.u32("lastMacroAction", s.lastMacroAction);
  jw.string("lastMacroResult", s.lastMacroResult ? s.lastMacroResult : "");
  jw.u32("lastMacroAgeMs", s.macroEvents ? (uint32_t)(now - s.lastMacroMs) : 0u);
  jw.u32("saveFailures", saveFailures);
  jw.endObject();
  jw.finish();
  return j;
}

static void bcSendError(int code, const char *error) {
  String j = "{\"ok\":false,\"error\":\"";
  j += error;
  j += "\"}";
  server.send(code, "application/json", j);
}

static bool bcParseBoolArg(const char *name, bool &out, bool &present) {
  present = server.hasArg(name);
  if (!present) return true;
  const String v = server.arg(name);
  if (v == "1" || v == "true" || v == "on") { out = true; return true; }
  if (v == "0" || v == "false" || v == "off") { out = false; return true; }
  return false;
}

static bool bcParseUIntArg(const char *name, uint32_t lo, uint32_t hi, uint32_t &out, bool &present) {
  present = server.hasArg(name);
  if (!present) return true;
  const String v = server.arg(name);
  if (v.length() == 0u || v.length() > 6u) return false;
  uint32_t value = 0;
  for (size_t i = 0; i < v.length(); ++i) {
    const char ch = v.charAt(i);
    if (ch < '0' || ch > '9') return false;
    value = value * 10u + (uint32_t)(ch - '0');
  }
  if (value < lo || value > hi) return false;
  out = value;
  return true;
}

static void httpBodyComfortConfig() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", bodyComfortJson());
}

static void httpBodyComfortUpdate() {
  if (bodyComfortActiveBus() == BC_BUS_NONE) {
    bcSendError(409, "Body Comfort requires Body + Chassis (Juniper) or Model Y L");
    return;
  }
  BodyComfortConfigPure next = bodyComfortCfgSnapshot();
  bool present = false, any = false, b = false;
  uint32_t u = 0;
#define BC_BOOL_FIELD(key, field) \
  if (!bcParseBoolArg(key, b, present)) { bcSendError(400, "invalid " key); return; } \
  if (present) { next.field = b; any = true; }
#define BC_UINT_FIELD(key, lo, hi, field, type) \
  if (!bcParseUIntArg(key, lo, hi, u, present)) { bcSendError(400, "invalid " key); return; } \
  if (present) { next.field = (type)u; any = true; }
  BC_BOOL_FIELD("eeEnabled", eeEnabled)
  BC_UINT_FIELD("eeDoorMask", 0u, 15u, eeDoorMask, uint8_t)
  BC_BOOL_FIELD("eeWeatherBlock", eeWeatherBlock)
  BC_BOOL_FIELD("eeWeatherStrict", eeWeatherStrict)
  BC_UINT_FIELD("eeRestoreDelayMs", 0u, 5000u, eeRestoreDelayMs, uint16_t)
  BC_BOOL_FIELD("eeOpenPercent", eeOpenPercent)
  BC_BOOL_FIELD("eeMirrorFold", eeMirrorFold)
  BC_BOOL_FIELD("eeMirrorCamera", eeMirrorCamera)
  BC_UINT_FIELD("eeUnfoldKph", 1u, 80u, eeUnfoldKph, uint8_t)
  BC_UINT_FIELD("eeUnfoldHoldS", 0u, 30u, eeUnfoldHoldS, uint8_t)
  BC_BOOL_FIELD("sbEnabled", sbEnabled)
  BC_UINT_FIELD("sbSeatMask", 0u, 7u, sbSeatMask, uint8_t)
  BC_BOOL_FIELD("mcEnabled", mcEnabled)
  BC_UINT_FIELD("mcLongMs", 500u, 2000u, mcLongMs, uint16_t)
  BC_UINT_FIELD("mcGapMs", 200u, 800u, mcGapMs, uint16_t)
  BC_BOOL_FIELD("hzBlinkEnabled", hzBlinkEnabled)
  BC_UINT_FIELD("hzBlinkSeconds", 1u, 30u, hzBlinkSeconds, uint8_t)
  BC_BOOL_FIELD("rvHazard", rvHazard)
  BC_BOOL_FIELD("rvLight", rvLight)
  BC_UINT_FIELD("rvLightMode", 0u, 2u, rvLightMode, uint8_t)
  BC_BOOL_FIELD("drHazard", drHazard)
  BC_UINT_FIELD("drOffDelayMs", 0u, 10000u, drOffDelayMs, uint16_t)
  BC_BOOL_FIELD("kdEnabled", kdEnabled)
  BC_UINT_FIELD("kdTriggerPct", 10u, 100u, kdTriggerPct, uint8_t)
  BC_UINT_FIELD("kdTargetRaw", 1u, 2u, kdTargetRaw, uint8_t)
  BC_BOOL_FIELD("kdReleaseOnPedal", kdReleaseOnPedal)
  BC_UINT_FIELD("kdReleasePct", 0u, 90u, kdReleasePct, uint8_t)
  BC_BOOL_FIELD("kdReleaseOnBrake", kdReleaseOnBrake)
  BC_UINT_FIELD("kdReleaseMode", 0u, 1u, kdReleaseMode, uint8_t)
  BC_BOOL_FIELD("dhEnabled", dhEnabled)
  BC_UINT_FIELD("dhDoorMask", 0u, 3u, dhDoorMask, uint8_t)
  BC_UINT_FIELD("dhHoldS", 1u, 10u, dhHoldS, uint8_t)
  BC_UINT_FIELD("dhAction", 0u, BC_ACTION_COUNT - 1u, dhAction, uint8_t)
#undef BC_BOOL_FIELD
#undef BC_UINT_FIELD
  // Macro slot: mc=<button>,<gesture>,<action>
  if (server.hasArg("mc")) {
    const String v = server.arg("mc");
    int first = v.indexOf(','), second = first < 0 ? -1 : v.indexOf(',', first + 1);
    if (first <= 0 || second <= first + 1 || second >= (int)v.length() - 1) {
      bcSendError(400, "invalid mc");
      return;
    }
    const long button = v.substring(0, first).toInt();
    const long gesture = v.substring(first + 1, second).toInt();
    const long action = v.substring(second + 1).toInt();
    for (size_t i = 0; i < v.length(); ++i) {
      const char ch = v.charAt(i);
      if (ch != ',' && (ch < '0' || ch > '9')) { bcSendError(400, "invalid mc"); return; }
    }
    if (button < 0 || button >= BC_BTN_COUNT || gesture < 0 || gesture >= BC_GESTURE_COUNT ||
        action < 0 || action >= BC_ACTION_COUNT || action == BC_ACTION_RETIRED_4) {
      bcSendError(400, "invalid mc");
      return;
    }
    next.mcAction[button][gesture] = (uint8_t)action;
    any = true;
  }
  if (!any) { bcSendError(400, "no settings"); return; }
  if (next.dhAction == BC_ACTION_RETIRED_4) { bcSendError(400, "invalid dhAction"); return; }
  if (next.kdReleasePct + 5u > next.kdTriggerPct) {
    bcSendError(400, "kickdown release must be at least 5% below the trigger");
    return;
  }
  if (!next.kdReleaseOnPedal && !next.kdReleaseOnBrake) {
    bcSendError(400, "choose at least one kickdown release condition");
    return;
  }
  bodyComfortSanitizePure(next);
  if (!bodyComfortCfgPersist(next)) { bcSendError(500, "Settings could not be saved"); return; }
  portENTER_CRITICAL(&bcMux);
  bcCfg = next;
  bcCfgDirty = false;
  if (!next.eeEnabled) {
    for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i)
      if (!bcState.entry[i].inFlight && bcState.entry[i].phase == BC_ENTRY_OPEN_PENDING)
        bcState.entry[i].phase = BC_ENTRY_IDLE;
  }
  if (!next.mcEnabled) bcState.macroCount = 0;
  portEXIT_CRITICAL(&bcMux);
  server.send(200, "application/json", bodyComfortJson());
}

// Manual window test (P only): one window command through the Easy Entry path.
// POST /api/body/window-test?door=0..3&req=2|3|4  (2 VENT, 3 CLOSED, 4 OPEN)
static void httpBodyComfortWindowTest() {
  if (bodyComfortActiveBus() == BC_BUS_NONE) {
    bcSendError(409, "Body Comfort requires Body + Chassis (Juniper) or Model Y L");
    return;
  }
  uint32_t door = 0, req = 0;
  bool present = false, present2 = false;
  if (!bcParseUIntArg("door", 0u, 3u, door, present) || !present ||
      !bcParseUIntArg("req", 2u, 4u, req, present2) || !present2) {
    bcSendError(400, "door 0-3 and req 2 (vent), 3 (close), 4 (open) required");
    return;
  }
  const uint32_t now = (uint32_t)millis();
  const char *reason = nullptr;
  portENTER_CRITICAL(&bcMux);
  uint8_t gear = BC_GEAR_INVALID;
  if (!bcGearFreshLocked(now, gear) || gear != BC_GEAR_P) reason = "window test only in P";
  else if (bcState.winCmd.active) reason = "window command busy";
  else {
    uint8_t request = (uint8_t)req, pct = 0;
    if (request == BC_WINREQ_OPEN && bcCfg.eeOpenPercent) { request = BC_WINREQ_PERCENT; pct = 100u; }
    bcState.winCmdMask = (uint8_t)(1u << door);
    bcState.winCmdRequest = request;
    bcState.winCmdPct = pct;
    bcState.winCmdStockSent = 0;
    bcState.winCmdSynthSent = 0;
    bcState.winCmdStockSeen = false;
    bcState.winCmd.active = true;
    bcState.winCmd.framesLeft = BC_WINDOW_CMD_FRAMES;
    bcState.winCmd.startMs = now;
  }
  portEXIT_CRITICAL(&bcMux);
  if (reason) { bcSendError(409, reason); return; }
  server.send(200, "application/json", bodyComfortJson());
}

static void httpBodyComfortAction() {
  if (bodyComfortActiveBus() == BC_BUS_NONE) {
    bcSendError(409, "Body Comfort requires Body + Chassis (Juniper) or Model Y L");
    return;
  }
  uint32_t id = 0;
  bool present = false;
  if (!bcParseUIntArg("id", 1u, BC_ACTION_COUNT - 1u, id, present) || !present) {
    bcSendError(400, "invalid id");
    return;
  }
  if (id == BC_ACTION_RETIRED_4) {
    bcSendError(400, "invalid id");
    return;
  }
  portENTER_CRITICAL(&bcMux);
  const char *reason = bcRunActionLocked((uint32_t)millis(), (uint8_t)id);
  portEXIT_CRITICAL(&bcMux);
  if (reason == BC_DEFERRED_EXISTING) reason = bodyComfortToggleExistingFeature((uint8_t)id);
  if (reason != nullptr) {
    bcSendError(409, reason);
    return;
  }
  server.send(200, "application/json", bodyComfortJson());
}
