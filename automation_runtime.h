#pragma once
// Automation runtime (phase 1). Include right after body_comfort_runtime.h.
//
// Each Body Comfort tick (10 ms) calls automationServiceTick(); every 50 ms it
// takes a snapshot of the vehicle signals Body Comfort already decodes,
// steps each rule's trigger, checks its AND / OR conditions and runs its
// action list. Actions reuse the Body Comfort command paths (window / mirror /
// frunk one-shots, hazard and camera button presses, rear fog override), so
// the CAN TX barrier, listen-only and every existing safety gate still apply.
// Actions in AUTO_DRIVE_BLOCKLIST never run in D or without a fresh gear.
// Automation state is guarded by bcMux, like Body Comfort.
#include "automation_pure.h"

static constexpr const char *AUTO_NVS_NAMESPACE = "bcauto";
static constexpr const char *AUTO_NVS_RULES = "rules";
static constexpr const char *AUTO_NVS_GLOBAL = "glob";
static constexpr uint32_t AUTO_TICK_MS = 50u;
static constexpr uint32_t AUTO_BUSY_RETRY_MS = 3000u;
static constexpr uint8_t AUTO_RATE_MAX_PER_MIN = 30u;
static constexpr uint8_t AUTO_LOG_LEN = 20u;
static constexpr uint32_t AUTO_GLOBAL_SAVE_DEBOUNCE_MS = 2000u;
static constexpr uint8_t AUTO_DEFERRED_MAX = AUTO_RULE_MAX;
static constexpr uint8_t AUTO_LOG_TRIGGERED = 0xFFu;
static constexpr uint8_t AUTO_LOG_MANUAL = 0xFEu;
static constexpr uint8_t AUTO_CSV_FIELDS = 4u + 6u + AUTO_COND_MAX * 4u + AUTO_ACT_MAX * 3u;
static_assert(AUTO_CSV_FIELDS == 35u, "rule csv layout");

struct AutoRuleRuntime {
  AutoTriggerStatePure trig;
  bool running;
  bool manual;
  uint8_t step;
  uint32_t dueMs;
  bool busy;
  uint32_t busySinceMs;
  bool firedOnce;
  uint32_t lastFireMs;
  uint32_t fires, skippedCond, skippedCooldown, skippedBusy;
  const char *lastResult;
};

struct AutoLogEntryRuntime {
  uint32_t ms;
  uint8_t rule;
  uint8_t type;   // AutoActionTypePure, or AUTO_LOG_TRIGGERED / AUTO_LOG_MANUAL
  uint8_t a;
  uint16_t b;
  bool dry;
  const char *result;
};

struct AutoDeferredRuntime {
  uint8_t rule;
  AutoActionPure act;
};

static AutoRulePure autoRules[AUTO_RULE_MAX];
static AutoRuleRuntime autoRt[AUTO_RULE_MAX];
static bool autoEnabled = false;
static uint8_t autoFlags = 0;
static bool autoGlobalDirty = false;
static uint32_t autoGlobalDirtyMs = 0;
static AutoLogEntryRuntime autoLog[AUTO_LOG_LEN];
static uint8_t autoLogHead = 0, autoLogCount = 0;
static uint32_t autoRateWindowMs = 0;
static uint8_t autoRateCount = 0;
static uint32_t autoRateLimited = 0;
static bool autoTicked = false;
static uint32_t autoLastTickMs = 0;
static uint32_t autoSaveFailures = 0;
static uint32_t autoActionsRun = 0;
static uint32_t autoDriveBlocked = 0;
static bool autoExistingOn[AUTO_FEAT_COUNT];
static bool autoListenOnlySnapshot = false;

static const char *const AUTO_RES_OK = "OK";
static const char *const AUTO_RES_DRY = "dry-run (not sent)";
static const char *const AUTO_RES_BLOCKED_DRIVE = "blocked in D";
static const char *const AUTO_RES_LISTEN_ONLY = "listen-only (not sent)";
static const char *const AUTO_RES_RATE = "rate limited";
static const char *const AUTO_RES_BUSY_GAVE_UP = "busy (gave up)";
static const char *const AUTO_RES_TRIGGERED = "triggered";
static const char *const AUTO_RES_MANUAL = "run now";
static const char *const AUTO_RES_DEFERRED = "deferred";
static const char *const AUTO_RES_BUSY = "busy";

// ─── NVS ─────────────────────────────────────────────────────────────────
static bool automationPersistRules(const AutoRulePure *rules) {
  static uint8_t blob[AUTO_BLOB_MAX];  // web task only
  const uint16_t len = autoSerializePure(rules, blob);
  Preferences p;
  if (!p.begin(AUTO_NVS_NAMESPACE, false)) return false;
  const size_t written = p.putBytes(AUTO_NVS_RULES, blob, len);
  p.end();
  return written == len;
}

static bool automationPersistGlobal(bool enabled, uint8_t flags) {
  uint8_t blob[AUTO_GLOBAL_BYTES];
  autoGlobalEncodePure(enabled, flags, blob);
  Preferences p;
  if (!p.begin(AUTO_NVS_NAMESPACE, false)) return false;
  const size_t written = p.putBytes(AUTO_NVS_GLOBAL, blob, sizeof(blob));
  p.end();
  return written == sizeof(blob);
}

static void automationLoad() {
  static AutoRulePure rules[AUTO_RULE_MAX];  // boot / tests only
  for (uint8_t i = 0; i < AUTO_RULE_MAX; ++i) rules[i] = {};
  bool enabled = false;
  uint8_t flags = 0;
  Preferences p;
  if (p.begin(AUTO_NVS_NAMESPACE, true)) {
    static uint8_t blob[AUTO_BLOB_MAX];
    size_t len = p.getBytesLength(AUTO_NVS_RULES);
    if (len > 0u && len <= sizeof(blob) && p.getBytes(AUTO_NVS_RULES, blob, len) == len)
      (void)autoDeserializePure(blob, (uint16_t)len, rules);
    uint8_t g[AUTO_GLOBAL_BYTES] = {};
    len = p.getBytesLength(AUTO_NVS_GLOBAL);
    if (len == sizeof(g) && p.getBytes(AUTO_NVS_GLOBAL, g, len) == len)
      (void)autoGlobalDecodePure(g, (uint16_t)len, enabled, flags);
    p.end();
  }
  uint8_t count = 0;
  portENTER_CRITICAL(&bcMux);
  for (uint8_t i = 0; i < AUTO_RULE_MAX; ++i) {
    autoRules[i] = rules[i];
    autoRt[i] = {};
    if (rules[i].used) count++;
  }
  autoEnabled = enabled;
  autoFlags = flags;
  autoGlobalDirty = false;
  portEXIT_CRITICAL(&bcMux);
  T2CAN_SERIAL_PRINTF("Automation: %s, %u rule(s)\n", enabled ? "ON" : "OFF", (unsigned)count);
}

// ─── Existing features (read / switch outside bcMux) ─────────────────────
static uint8_t autoExistingToggleAction(uint8_t feature) {
  switch (feature) {
    case AUTO_FEAT_NAG_KILLER: return BC_ACTION_TOGGLE_NAG_KILLER;
    case AUTO_FEAT_ISA: return BC_ACTION_TOGGLE_ISA_SUPPRESSION;
    case AUTO_FEAT_DMS: return BC_ACTION_TOGGLE_DRIVER_MONITORING;
    case AUTO_FEAT_AUTO_BLINKER: return BC_ACTION_TOGGLE_AUTO_BLINKER;
    case AUTO_FEAT_TLSSC: return BC_ACTION_TOGGLE_TLSSC;
    case AUTO_FEAT_DOOR_CANCEL: return BC_ACTION_TOGGLE_DOOR_CANCEL;
    case AUTO_FEAT_VISION: return BC_ACTION_TOGGLE_VISION_CONTROL;
    case AUTO_FEAT_CONT_AP: return BC_ACTION_TOGGLE_CONTINUOUS_AP;
    case AUTO_FEAT_PREHEAT: return BC_ACTION_TOGGLE_BATTERY_PREHEAT;
    case AUTO_FEAT_LISTEN_ONLY: return BC_ACTION_TOGGLE_LISTEN_ONLY;
    default: return BC_ACTION_NONE;
  }
}

static bool autoExistingState(uint8_t feature) {
  switch (feature) {
    case AUTO_FEAT_NAG_KILLER: {
      NagConfig nc;
      portENTER_CRITICAL(&nagCfgMux); nc = nagCfg; portEXIT_CRITICAL(&nagCfgMux);
      return nc.enabled;
    }
    case AUTO_FEAT_ISA: return isaSuppressionControlSnapshot();
    case AUTO_FEAT_DMS: return driverMonitoringControlSnapshot();
    case AUTO_FEAT_AUTO_BLINKER: return blinkAEnabled;
    case AUTO_FEAT_TLSSC: return tlsscEnabled;
    case AUTO_FEAT_DOOR_CANCEL: return featureCfgRuntimeSnapshot().door;
    case AUTO_FEAT_VISION: return !visionControlRequestDisabled();
    case AUTO_FEAT_CONT_AP: return continuousApControlSnapshot().enabled;
    case AUTO_FEAT_PREHEAT: return preheatEnabled;
    case AUTO_FEAT_LISTEN_ONLY: return canUsbPassive();
    default: return false;
  }
}

static bool autoFeatureAvailable(uint8_t feature) {
  switch (feature) {
    case AUTO_FEAT_NAG_KILLER: return activeProfileNagSupported();
    case AUTO_FEAT_ISA: return activeProfileIsaSuppressionSupported();
    case AUTO_FEAT_DMS: return activeProfileDmsNagSupported();
    case AUTO_FEAT_AUTO_BLINKER: return activeProfileAdvancedEapSupported();
    case AUTO_FEAT_DOOR_CANCEL: return activeProfileBodyControlsSupported() || activeProfileUsesYlTopology();
    case AUTO_FEAT_VISION: return visionControlSupported();
    case AUTO_FEAT_PREHEAT: return activeProfilePreheatSupported();
    case AUTO_FEAT_KICKDOWN: return activeProfilePedalMapSupported();
    default: return feature < AUTO_FEAT_COUNT;
  }
}

static bool *autoBcFeatureField(uint8_t feature) {
  switch (feature) {
    case AUTO_FEAT_EASY_ENTRY: return &bcCfg.eeEnabled;
    case AUTO_FEAT_EE_MIRROR_FOLD: return &bcCfg.eeMirrorFold;
    case AUTO_FEAT_SEATBELT: return &bcCfg.sbEnabled;
    case AUTO_FEAT_REVERSE_HAZARD: return &bcCfg.rvHazard;
    case AUTO_FEAT_REVERSE_LIGHT: return &bcCfg.rvLight;
    case AUTO_FEAT_DOOR_HAZARD: return &bcCfg.drHazard;
    case AUTO_FEAT_KICKDOWN: return &bcCfg.kdEnabled;
    case AUTO_FEAT_DOOR_HANDLE: return &bcCfg.dhEnabled;
    case AUTO_FEAT_MACROS: return &bcCfg.mcEnabled;
    case AUTO_FEAT_HAZARD_BLINK: return &bcCfg.hzBlinkEnabled;
    default: return nullptr;
  }
}

// ─── Signals ─────────────────────────────────────────────────────────────
static void autoSnapshotSignalsLocked(uint32_t now, AutoSignalsPure &s, bool &gearFresh, uint8_t &gear) {
  memset(&s, 0, sizeof(s));
  gear = BC_GEAR_INVALID;
  gearFresh = bcGearFreshLocked(now, gear) && gear >= BC_GEAR_P && gear <= BC_GEAR_D;
  autoSignalSetPure(s, AUTO_SIG_GEAR, gearFresh, gear);
  if (bcAgeFreshPure(now, bcState.speedMs, bcState.speedValid, BC_SPEED_FRESH_MS)) {
    int32_t kph = (bcState.kph10 + (bcState.kph10 >= 0 ? 5 : -5)) / 10;
    if (kph < -50) kph = -50;
    if (kph > 250) kph = 250;
    autoSignalSetPure(s, AUTO_SIG_SPEED, true, (int16_t)kph);
  }
  if (bcAgeFreshPure(now, bcState.pedalMs, bcState.pedalValid, BC_GEAR_FRESH_MS))
    autoSignalSetPure(s, AUTO_SIG_PEDAL, true, (int16_t)((bcState.pedalPct10 + 5u) / 10u));
  if (bcAgeFreshPure(now, bcState.brakeMs, bcState.brakeValid, BC_GEAR_FRESH_MS))
    autoSignalSetPure(s, AUTO_SIG_BRAKE, true, bcState.brakeOn ? 1 : 0);
  bool allDoors = true, anyOpen = false;
  for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
    const bool ok = bcDoorFreshLocked(now, i) && bcState.door[i] != BC_DOOR_STATE_UNKNOWN;
    const bool open = ok && bcState.door[i] == BC_DOOR_STATE_OPEN;
    autoSignalSetPure(s, (uint8_t)(AUTO_SIG_DOOR_FL + i), ok, open ? 1 : 0);
    if (!ok) allDoors = false;
    if (open) anyOpen = true;
  }
  autoSignalSetPure(s, AUTO_SIG_ANY_DOOR, allDoors || anyOpen, anyOpen ? 1 : 0);
  for (uint8_t side = 0; side < 2u; ++side)
    autoSignalSetPure(s, (uint8_t)(AUTO_SIG_HANDLE_FL + side),
                      bcAgeFreshPure(now, bcState.doorMs[side], bcState.doorValid[side], BC_DOOR_FRESH_MS),
                      bcState.handlePulled[side] ? 1 : 0);
  if (bcAgeFreshPure(now, bcState.lockMs, bcState.lockValid, BC_LOCK_FRESH_MS))
    autoSignalSetPure(s, AUTO_SIG_LOCKED, true, bcState.lock.simple == 2u ? 1 : 0);
  bool allWin = true, anyWinOpen = false;
  for (uint8_t i = 0; i < BC_DOOR_COUNT; ++i) {
    int16_t v = 0;
    const bool ok = bcWindowFreshLocked(now, i) && autoWindowValuePure(bcState.winPosState[i], v);
    autoSignalSetPure(s, (uint8_t)(AUTO_SIG_WIN_FL + i), ok, v);
    if (!ok) allWin = false;
    else if (v > 0) anyWinOpen = true;
  }
  autoSignalSetPure(s, AUTO_SIG_ANY_WIN_OPEN, allWin || anyWinOpen, anyWinOpen ? 1 : 0);
  if (bcAgeFreshPure(now, bcState.weatherMs, bcState.weatherValid, BC_WEATHER_FRESH_MS)) {
    autoSignalSetPure(s, AUTO_SIG_RAIN, true, bcState.rainy ? 1 : 0);
    autoSignalSetPure(s, AUTO_SIG_SNOW, true, bcState.snowy ? 1 : 0);
    autoSignalSetPure(s, AUTO_SIG_RAIN_OR_SNOW, true, (bcState.rainy || bcState.snowy) ? 1 : 0);
  }
  if (bcAgeFreshPure(now, bcState.doorMs[0], bcState.doorValid[0], BC_DOOR_FRESH_MS) &&
      bcAgeFreshPure(now, bcState.doorMs[1], bcState.doorValid[1], BC_DOOR_FRESH_MS)) {
    const uint8_t l = bcState.mirrorFold[0], r = bcState.mirrorFold[1];
    const bool moving = l == BC_MIRROR_FOLDING || r == BC_MIRROR_FOLDING || l == 4u || r == 4u;
    if (moving) autoSignalSetPure(s, AUTO_SIG_MIRRORS, true, 2);
    else if (l == BC_MIRROR_FOLDED || r == BC_MIRROR_FOLDED) autoSignalSetPure(s, AUTO_SIG_MIRRORS, true, 1);
    else if (l == BC_MIRROR_UNFOLDED && r == BC_MIRROR_UNFOLDED) autoSignalSetPure(s, AUTO_SIG_MIRRORS, true, 0);
  }
  if (bcAgeFreshPure(now, bcState.uiStatusMs, bcState.uiStatusValid, BC_UI_STATUS_FRESH_MS))
    autoSignalSetPure(s, AUTO_SIG_CAMERA, true, bcState.cameraActive ? 1 : 0);
  if (bcAgeFreshPure(now, bcState.lightingMs, bcState.lightingValid, BC_LIGHTING_FRESH_MS))
    autoSignalSetPure(s, AUTO_SIG_HAZARDS, true, bcState.hazardReq != BC_HAZARD_REQ_NONE ? 1 : 0);
  if (bcState.vc3b3Valid && (uint32_t)(now - bcState.vc3b3Ms) <= 1500u) {
    const uint8_t sw = (uint8_t)bcGetBitsPure(bcState.vc3b3Template, 9, 3);
    if (sw <= 3u) autoSignalSetPure(s, AUTO_SIG_LIGHT_SWITCH, true, sw);
  }
  if (bcAgeFreshPure(now, bcState.beltMs, bcState.beltValid, BC_GEAR_FRESH_MS))
    for (uint8_t i = 0; i < 3u; ++i)
      autoSignalSetPure(s, (uint8_t)(AUTO_SIG_BELT_2L + i), true, bcState.belt2[i] == 1u ? 1 : 0);
  for (uint8_t f = 0; f < AUTO_FEAT_COUNT; ++f) {
    const bool *field = autoBcFeatureField(f);
    const bool on = field ? *field : autoExistingOn[f];
    autoSignalSetPure(s, (uint8_t)(AUTO_SIG_FEATURE_BASE + f), true, on ? 1 : 0);
  }
  for (uint8_t i = 0; i < AUTO_FLAG_COUNT; ++i)
    autoSignalSetPure(s, (uint8_t)(AUTO_SIG_FLAG_BASE + i), true, ((autoFlags >> i) & 1u) ? 1 : 0);
}

// ─── Log ─────────────────────────────────────────────────────────────────
static void autoLogLocked(uint32_t now, uint8_t rule, uint8_t type, uint8_t a, uint16_t b, bool dry,
                          const char *result) {
  const uint8_t slot = (uint8_t)((autoLogHead + autoLogCount) % AUTO_LOG_LEN);
  autoLog[slot] = {now, rule, type, a, b, dry, result};
  if (autoLogCount < AUTO_LOG_LEN) autoLogCount++;
  else autoLogHead = (uint8_t)((autoLogHead + 1u) % AUTO_LOG_LEN);
  if (rule < AUTO_RULE_MAX && type != AUTO_LOG_TRIGGERED && type != AUTO_LOG_MANUAL)
    autoRt[rule].lastResult = result;
}

static bool autoRateTakeLocked(uint32_t now) {
  if ((uint32_t)(now - autoRateWindowMs) >= 60000u) { autoRateWindowMs = now; autoRateCount = 0; }
  if (autoRateCount >= AUTO_RATE_MAX_PER_MIN) { autoRateLimited++; return false; }
  autoRateCount++;
  return true;
}

// ─── Actions ─────────────────────────────────────────────────────────────
static bool autoActionDeferred(uint8_t type, uint8_t a) {
  if (type == AUTO_ACT_PEDAL_MODE) return true;
  return type == AUTO_ACT_FEATURE && autoBcFeatureField(a) == nullptr;
}

// Runs one action under bcMux. Returns AUTO_RES_BUSY to retry later,
// AUTO_RES_DEFERRED for actions that must run outside the lock, otherwise the
// result for the log.
static const char *autoRunActionLocked(uint32_t now, const AutoActionPure &a) {
  switch (a.type) {
    case AUTO_ACT_HAZARD_BLINK: {
      const char *r = bcStartHazardBlinkForLocked(now, (uint8_t)a.b, true);
      if (r && strcmp(r, "already running") == 0) return AUTO_RES_BUSY;
      return r ? r : AUTO_RES_OK;
    }
    case AUTO_ACT_MIRRORS: {
      if (bcState.mirrorCmd.active) return AUTO_RES_BUSY;
      const bool folded = bcState.mirrorFold[0] == BC_MIRROR_FOLDED || bcState.mirrorFold[1] == BC_MIRROR_FOLDED;
      if (a.a == 1u) {
        if (folded) return "already folded";
        bcMirrorCommandLocked(now, BC_MIRROR_REQ_RETRACT);
      } else {
        if (!folded && !bcState.mirrorFoldedByUs) return "already unfolded";
        bcMirrorCommandLocked(now, BC_MIRROR_REQ_PRESENT);
      }
      return AUTO_RES_OK;
    }
    case AUTO_ACT_WINDOWS: {
      const char *r = bcWindowCommandStartLocked(now, a.a, (uint8_t)a.b);
      return r ? AUTO_RES_BUSY : AUTO_RES_OK;
    }
    case AUTO_ACT_CAMERA:
      return bcCameraPressStartLocked(now) ? AUTO_RES_OK : AUTO_RES_BUSY;
    case AUTO_ACT_FRUNK: {
      const char *r = bcRunActionLocked(now, BC_ACTION_OPEN_FRUNK);
      if (r && strcmp(r, "frunk command busy") == 0) return AUTO_RES_BUSY;
      return r ? r : AUTO_RES_OK;
    }
    case AUTO_ACT_REAR_FOG:
      if (a.a == 1u) {
        bcState.autoFogActive = true;
        bcState.autoFogUntilMs = now + (uint32_t)a.b * 1000u;
        bcState.autoFogStarts++;
      } else {
        bcState.autoFogActive = false;
      }
      return AUTO_RES_OK;
    case AUTO_ACT_FEATURE: {
      bool *field = autoBcFeatureField(a.a);
      if (field == nullptr) return AUTO_RES_DEFERRED;
      const bool next = a.b == 2u ? !*field : a.b == 1u;
      if (next == *field) return "already set";
      *field = next;
      bcCfgDirty = true;
      bcCfgDirtyMs = now;
      return AUTO_RES_OK;
    }
    case AUTO_ACT_PEDAL_MODE:
      return AUTO_RES_DEFERRED;
    case AUTO_ACT_FLAG: {
      const uint8_t bit = (uint8_t)(1u << a.a);
      const uint8_t next = a.b == 2u ? (uint8_t)(autoFlags ^ bit)
                         : a.b == 1u ? (uint8_t)(autoFlags | bit) : (uint8_t)(autoFlags & ~bit);
      if (next == autoFlags) return "already set";
      autoFlags = next;
      autoGlobalDirty = true;
      autoGlobalDirtyMs = now;
      return AUTO_RES_OK;
    }
    default:
      return "no action";
  }
}

// Outside bcMux: existing-feature switches and the pedal map request.
static const char *autoRunDeferredAction(const AutoActionPure &a) {
  if (a.type == AUTO_ACT_PEDAL_MODE) {
    if (!activeProfilePedalMapSupported()) return "pedal mode unavailable";
    static const uint8_t raw[4] = {PEDAL_MAP_RAW_CHILL, PEDAL_MAP_RAW_SPORT, PEDAL_MAP_RAW_PERFORMANCE,
                                   PEDAL_MAP_RAW_STOCK};
    return requestPedalMapMode(raw[a.a & 3u], "Automation") ? AUTO_RES_OK : "pedal mode refused";
  }
  if (a.type != AUTO_ACT_FEATURE) return "no action";
  const uint8_t toggle = autoExistingToggleAction(a.a);
  if (toggle == BC_ACTION_NONE) return "no action";
  if (a.b != 2u && autoExistingState(a.a) == (a.b == 1u)) return "already set";
  const char *r = bodyComfortToggleExistingFeature(toggle);
  return r ? r : AUTO_RES_OK;
}

// Advances one rule's action list. Deferred actions are queued and the list
// continues on the next tick, so actions keep their order.
static void autoRunnerStepLocked(uint32_t now, uint8_t idx, bool gearFresh, uint8_t gear,
                                 AutoDeferredRuntime *deferred, uint8_t &deferredCount) {
  AutoRuleRuntime &rt = autoRt[idx];
  const AutoRulePure &rule = autoRules[idx];
  while (rt.running && rt.step < AUTO_ACT_MAX && (int32_t)(now - rt.dueMs) >= 0) {
    const AutoActionPure &a = rule.act[rt.step];
    if (a.type == AUTO_ACT_NONE) { rt.step++; continue; }
    if (a.type == AUTO_ACT_DELAY) {
      rt.dueMs = now + (uint32_t)a.b * 1000u;
      rt.step++;
      continue;
    }
    const char *result = nullptr;
    if (!autoActionAllowedInGearPure(a, gearFresh, gear)) {
      result = AUTO_RES_BLOCKED_DRIVE;
      autoDriveBlocked++;
    } else if (rule.dryRun) {
      result = AUTO_RES_DRY;
    } else if (autoListenOnlySnapshot && autoActionSendsCanPure(a.type)) {
      result = AUTO_RES_LISTEN_ONLY;
    } else if (autoActionDeferred(a.type, a.a)) {
      if (deferredCount >= AUTO_DEFERRED_MAX) return;  // retry next tick
      if (!autoRateTakeLocked(now)) {
        result = AUTO_RES_RATE;
      } else {
        deferred[deferredCount++] = {idx, a};
        rt.step++;
        rt.busy = false;
        return;  // the rest continues after the deferred action ran
      }
    } else if (!rt.busy && !autoRateTakeLocked(now)) {
      result = AUTO_RES_RATE;
    } else {
      result = autoRunActionLocked(now, a);
      if (result == AUTO_RES_BUSY) {
        if (!rt.busy) { rt.busy = true; rt.busySinceMs = now; }
        if ((uint32_t)(now - rt.busySinceMs) < AUTO_BUSY_RETRY_MS) return;
        result = AUTO_RES_BUSY_GAVE_UP;
      } else if (result == AUTO_RES_OK) {
        autoActionsRun++;
      }
    }
    rt.busy = false;
    autoLogLocked(now, idx, a.type, a.a, a.b, rule.dryRun, result);
    rt.step++;
  }
  if (rt.step >= AUTO_ACT_MAX && (int32_t)(now - rt.dueMs) >= 0) rt.running = false;
}

static void autoStartRunnerLocked(uint32_t now, uint8_t idx, bool manual) {
  AutoRuleRuntime &rt = autoRt[idx];
  rt.running = true;
  rt.manual = manual;
  rt.step = 0;
  rt.dueMs = now;
  rt.busy = false;
  if (!manual) {
    rt.firedOnce = true;
    rt.lastFireMs = now;
    rt.fires++;
  }
  autoLogLocked(now, idx, manual ? AUTO_LOG_MANUAL : AUTO_LOG_TRIGGERED, 0, 0, autoRules[idx].dryRun,
                manual ? AUTO_RES_MANUAL : AUTO_RES_TRIGGERED);
}

static void automationServiceTick(uint32_t now) {
  if (autoTicked && (uint32_t)(now - autoLastTickMs) < AUTO_TICK_MS) return;
  autoTicked = true;
  autoLastTickMs = now;
  // Existing-feature states are read outside bcMux (they use their own locks).
  bool existing[AUTO_FEAT_COUNT] = {};
  for (uint8_t f = AUTO_FEAT_EXISTING_FIRST; f < AUTO_FEAT_COUNT; ++f) existing[f] = autoExistingState(f);
  const bool listenOnly = canUsbPassive();

  AutoDeferredRuntime deferred[AUTO_DEFERRED_MAX];
  uint8_t deferredCount = 0;
  bool saveGlobal = false, enabledNow = false;
  uint8_t flagsNow = 0;
  portENTER_CRITICAL(&bcMux);
  memcpy(autoExistingOn, existing, sizeof(existing));
  autoListenOnlySnapshot = listenOnly;
  AutoSignalsPure sig;
  bool gearFresh = false;
  uint8_t gear = BC_GEAR_INVALID;
  autoSnapshotSignalsLocked(now, sig, gearFresh, gear);
  for (uint8_t i = 0; i < AUTO_RULE_MAX; ++i) {
    const AutoRulePure &rule = autoRules[i];
    AutoRuleRuntime &rt = autoRt[i];
    if (!rule.used) continue;
    const uint8_t ts = rule.trig.signal < AUTO_SIG_SLOTS ? rule.trig.signal : (uint8_t)AUTO_SIG_NONE;
    // Triggers always track the signal so enabling a rule never fires on an old edge.
    const bool fired = autoTriggerStepPure(rt.trig, rule.trig, now, sig.valid[ts], sig.value[ts]);
    if (fired && autoEnabled && rule.enabled) {
      if (rt.firedOnce && (uint32_t)(now - rt.lastFireMs) < (uint32_t)rule.cooldownS * 1000u) {
        rt.skippedCooldown++;
      } else if (!autoConditionsPassPure(rule, sig)) {
        rt.skippedCond++;
        rt.lastResult = "conditions not met";
      } else if (rt.running && !rule.restart) {
        rt.skippedBusy++;
      } else {
        autoStartRunnerLocked(now, i, false);
      }
    }
    if (rt.running && !rt.manual && (!autoEnabled || !rule.enabled)) rt.running = false;
    if (rt.running) autoRunnerStepLocked(now, i, gearFresh, gear, deferred, deferredCount);
  }
  if (autoGlobalDirty && (uint32_t)(now - autoGlobalDirtyMs) >= AUTO_GLOBAL_SAVE_DEBOUNCE_MS) {
    autoGlobalDirty = false;
    saveGlobal = true;
    enabledNow = autoEnabled;
    flagsNow = autoFlags;
  }
  portEXIT_CRITICAL(&bcMux);

  for (uint8_t i = 0; i < deferredCount; ++i) {
    const char *r = autoRunDeferredAction(deferred[i].act);
    portENTER_CRITICAL(&bcMux);
    if (r == AUTO_RES_OK) autoActionsRun++;
    autoLogLocked(now, deferred[i].rule, deferred[i].act.type, deferred[i].act.a, deferred[i].act.b, false, r);
    portEXIT_CRITICAL(&bcMux);
  }
  if (saveGlobal && !automationPersistGlobal(enabledNow, flagsNow)) {
    portENTER_CRITICAL(&bcMux);
    autoSaveFailures++;
    portEXIT_CRITICAL(&bcMux);
  }
}

// ─── HTTP ────────────────────────────────────────────────────────────────
static void autoRuleCsv(const AutoRulePure &r, String &out) {
  int32_t v[AUTO_CSV_FIELDS];
  uint8_t n = 0;
  v[n++] = r.enabled; v[n++] = r.dryRun; v[n++] = r.restart; v[n++] = r.cooldownS;
  v[n++] = r.trig.type; v[n++] = r.trig.signal; v[n++] = r.trig.op;
  v[n++] = r.trig.value; v[n++] = r.trig.from; v[n++] = r.trig.holdS;
  for (uint8_t c = 0; c < AUTO_COND_MAX; ++c) {
    v[n++] = r.cond[c].signal; v[n++] = r.cond[c].opFlags & 0x7Fu;
    v[n++] = (r.cond[c].opFlags & AUTO_COND_OR) ? 1 : 0; v[n++] = r.cond[c].value;
  }
  for (uint8_t a = 0; a < AUTO_ACT_MAX; ++a) { v[n++] = r.act[a].type; v[n++] = r.act[a].a; v[n++] = r.act[a].b; }
  out += '[';
  for (uint8_t i = 0; i < n; ++i) { if (i) out += ','; out += (long)v[i]; }
  out += ']';
}

// Parses the 35-field rule CSV. Returns nullptr on success.
static const char *autoRuleFromCsv(const String &csv, AutoRulePure &r) {
  int32_t v[AUTO_CSV_FIELDS];
  uint8_t n = 0;
  size_t i = 0;
  const size_t len = csv.length();
  if (len == 0u || len > 400u) return "rule data";
  while (i <= len) {
    if (n >= AUTO_CSV_FIELDS) return "rule data: too many fields";
    bool neg = false, digits = false;
    int32_t x = 0;
    if (i < len && csv.charAt(i) == '-') { neg = true; ++i; }
    while (i < len && csv.charAt(i) >= '0' && csv.charAt(i) <= '9') {
      x = x * 10 + (csv.charAt(i) - '0');
      if (x > 40000) return "rule data: value out of range";
      digits = true;
      ++i;
    }
    if (!digits) return "rule data: number expected";
    v[n++] = neg ? -x : x;
    if (i == len) break;
    if (csv.charAt(i) != ',') return "rule data: comma expected";
    ++i;
  }
  if (n != AUTO_CSV_FIELDS) return "rule data: 35 fields required";
  for (uint8_t k = 0; k < AUTO_CSV_FIELDS; ++k)
    if (v[k] < -32768 || v[k] > 32767) return "rule data: value out of range";
  n = 0;
  r = {};
  r.used = true;
  if (v[0] < 0 || v[0] > 1 || v[1] < 0 || v[1] > 1 || v[2] < 0 || v[2] > 1) return "rule flags";
  r.enabled = v[n++] != 0; r.dryRun = v[n++] != 0; r.restart = v[n++] != 0;
  if (v[n] < 0) return "cooldown 1-3600 s";
  r.cooldownS = (uint16_t)v[n++];
  for (uint8_t k = 4; k < 7; ++k) if (v[k] < 0 || v[k] > 255) return "trigger";
  if (v[9] < 0 || v[9] > 255) return "hold 0-250 s";
  r.trig.type = (uint8_t)v[n++]; r.trig.signal = (uint8_t)v[n++]; r.trig.op = (uint8_t)v[n++];
  r.trig.value = (int16_t)v[n++]; r.trig.from = (int16_t)v[n++]; r.trig.holdS = (uint8_t)v[n++];
  for (uint8_t c = 0; c < AUTO_COND_MAX; ++c) {
    const int32_t sigv = v[n++], op = v[n++], orv = v[n++], val = v[n++];
    if (sigv < 0 || sigv > 255 || op < 0 || op > 7 || orv < 0 || orv > 1) return "condition";
    r.cond[c].signal = (uint8_t)sigv;
    r.cond[c].opFlags = (uint8_t)(op | (orv ? AUTO_COND_OR : 0u));
    r.cond[c].value = (int16_t)val;
    if (r.cond[c].signal == AUTO_SIG_NONE) r.cond[c] = {};
  }
  for (uint8_t a = 0; a < AUTO_ACT_MAX; ++a) {
    const int32_t t = v[n++], pa = v[n++], pb = v[n++];
    if (t < 0 || t > 255 || pa < 0 || pa > 255 || pb < 0) return "action";
    r.act[a] = {(uint8_t)t, (uint8_t)pa, (uint16_t)pb};
    if (r.act[a].type == AUTO_ACT_NONE) r.act[a] = {};
  }
  return autoRuleValidPure(r);
}

// HTTP handlers run only on the web task: static copies keep its stack small.
static String automationJson() {
  static AutoRulePure rules[AUTO_RULE_MAX];
  static AutoRuleRuntime rt[AUTO_RULE_MAX];
  static AutoLogEntryRuntime log[AUTO_LOG_LEN];
  uint8_t logHead, logCount, flags;
  bool enabled;
  uint32_t rateLimited, saveFailures, actionsRun, driveBlocked, fogStarts;
  bool fogActive;
  uint32_t fogLeftMs;
  const uint32_t now = (uint32_t)millis();
  portENTER_CRITICAL(&bcMux);
  memcpy(rules, autoRules, sizeof(rules));
  memcpy(rt, autoRt, sizeof(rt));
  memcpy(log, autoLog, sizeof(log));
  logHead = autoLogHead; logCount = autoLogCount; flags = autoFlags; enabled = autoEnabled;
  rateLimited = autoRateLimited; saveFailures = autoSaveFailures; actionsRun = autoActionsRun;
  driveBlocked = autoDriveBlocked; fogStarts = bcState.autoFogStarts; fogActive = bcState.autoFogActive;
  fogLeftMs = fogActive && (int32_t)(bcState.autoFogUntilMs - now) > 0 ? bcState.autoFogUntilMs - now : 0u;
  portEXIT_CRITICAL(&bcMux);

  String s;
  s.reserve(4096);
  JsonWriterArduino jw(s);
  jw.boolean("ok", true);
  jw.boolean("supported", bodyComfortActiveBus() != BC_BUS_NONE);
  jw.boolean("enabled", enabled);
  jw.boolean("listenOnly", canUsbPassive());
  jw.u32("flags", flags);
  jw.u32("ruleMax", AUTO_RULE_MAX);
  jw.beginArray("rules");
  bool first = true;
  for (uint8_t i = 0; i < AUTO_RULE_MAX; ++i) {
    if (!rules[i].used) continue;
    if (!first) s += ',';
    first = false;
    JsonWriterArduino rw(s);
    rw.u32("idx", i);
    rw.string("name", rules[i].name);
    String csv;
    autoRuleCsv(rules[i], csv);
    rw.raw("r", csv);
    rw.u32("fires", rt[i].fires);
    rw.i32("lastAgoS", rt[i].firedOnce ? (int32_t)((now - rt[i].lastFireMs) / 1000u) : -1);
    rw.boolean("running", rt[i].running);
    rw.u32("skippedCond", rt[i].skippedCond);
    rw.u32("skippedCooldown", rt[i].skippedCooldown);
    rw.u32("skippedBusy", rt[i].skippedBusy);
    rw.string("last", rt[i].lastResult ? rt[i].lastResult : "");
    rw.finish();
  }
  jw.endArray();
  jw.beginArray("log");
  for (uint8_t k = 0; k < logCount; ++k) {
    const AutoLogEntryRuntime &e = log[(logHead + logCount - 1u - k) % AUTO_LOG_LEN];
    if (k) s += ',';
    JsonWriterArduino lw(s);
    lw.u32("agoS", (now - e.ms) / 1000u);
    lw.u32("rule", e.rule);
    lw.u32("type", e.type);
    lw.u32("a", e.a);
    lw.u32("b", e.b);
    lw.boolean("dry", e.dry);
    lw.string("result", e.result ? e.result : "");
    lw.finish();
  }
  jw.endArray();
  jw.beginObject("stats");
  jw.u32("actionsRun", actionsRun);
  jw.u32("driveBlocked", driveBlocked);
  jw.u32("rateLimited", rateLimited);
  jw.u32("saveFailures", saveFailures);
  jw.boolean("rearFogActive", fogActive);
  jw.u32("rearFogLeftS", (fogLeftMs + 999u) / 1000u);
  jw.u32("rearFogStarts", fogStarts);
  jw.endObject();
  jw.finish();
  return s;
}

static void httpAutomationGet() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", automationJson());
}

static void httpAutomationCatalog() {
  String s;
  s.reserve(2600);
  JsonWriterArduino jw(s);
  jw.boolean("ok", true);
  jw.beginArray("signals");
  bool first = true;
  for (uint16_t id = 1; id < AUTO_SIG_SLOTS; ++id) {
    AutoSignalInfoPure info;
    if (!autoSignalInfoPure((uint8_t)id, info)) continue;
    if (!first) s += ',';
    first = false;
    JsonWriterArduino w(s);
    w.u32("id", id);
    w.string("key", info.key);
    w.u32("kind", info.kind);
    w.i32("min", info.min);
    w.i32("max", info.max);
    w.finish();
  }
  jw.endArray();
  String feats = "[";
  for (uint8_t f = 0; f < AUTO_FEAT_COUNT; ++f) {
    if (f) feats += ',';
    feats += autoFeatureAvailable(f) ? "1" : "0";
  }
  feats += ']';
  jw.raw("featureAvailable", feats);
  String blocked = "[";
  for (uint8_t i = 0; i < AUTO_DRIVE_BLOCK_COUNT; ++i) {
    if (i) blocked += ',';
    blocked += '['; blocked += (unsigned)AUTO_DRIVE_BLOCKLIST[i].type; blocked += ',';
    blocked += (unsigned)AUTO_DRIVE_BLOCKLIST[i].a; blocked += ']';
  }
  blocked += ']';
  jw.raw("driveBlocked", blocked);
  jw.u32("ruleMax", AUTO_RULE_MAX);
  jw.u32("condMax", AUTO_COND_MAX);
  jw.u32("actMax", AUTO_ACT_MAX);
  jw.u32("flagCount", AUTO_FLAG_COUNT);
  jw.u32("nameBytes", AUTO_NAME_LEN - 1u);
  jw.finish();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}

static bool autoParseIdx(uint32_t &idx) {
  bool present = false;
  return bcParseUIntArg("idx", 0u, AUTO_RULE_MAX - 1u, idx, present) && present;
}

static void httpAutomationRule() {
  uint32_t idx = 0;
  if (!autoParseIdx(idx)) { bcSendError(400, "idx 0-15 required"); return; }
  if (!server.hasArg("r")) { bcSendError(400, "rule data required"); return; }
  AutoRulePure r;
  const char *why = autoRuleFromCsv(server.arg("r"), r);
  if (why) { bcSendError(400, why); return; }
  autoNameSanitizePure(server.arg("name").c_str(), r.name);
  static AutoRulePure next[AUTO_RULE_MAX];
  portENTER_CRITICAL(&bcMux);
  memcpy(next, autoRules, sizeof(next));
  portEXIT_CRITICAL(&bcMux);
  next[idx] = r;
  if (!automationPersistRules(next)) { bcSendError(500, "save failed"); return; }
  portENTER_CRITICAL(&bcMux);
  autoRules[idx] = r;
  autoRt[idx] = {};
  portEXIT_CRITICAL(&bcMux);
  server.send(200, "application/json", automationJson());
}

static void httpAutomationDelete() {
  uint32_t idx = 0;
  if (!autoParseIdx(idx)) { bcSendError(400, "idx 0-15 required"); return; }
  static AutoRulePure next[AUTO_RULE_MAX];
  portENTER_CRITICAL(&bcMux);
  memcpy(next, autoRules, sizeof(next));
  portEXIT_CRITICAL(&bcMux);
  next[idx] = {};
  if (!automationPersistRules(next)) { bcSendError(500, "save failed"); return; }
  portENTER_CRITICAL(&bcMux);
  autoRules[idx] = {};
  autoRt[idx] = {};
  portEXIT_CRITICAL(&bcMux);
  server.send(200, "application/json", automationJson());
}

static void httpAutomationGlobal() {
  bool enabled = false, present = false;
  if (!bcParseBoolArg("enabled", enabled, present) || !present) { bcSendError(400, "enabled=0|1 required"); return; }
  uint8_t flags;
  portENTER_CRITICAL(&bcMux);
  flags = autoFlags;
  portEXIT_CRITICAL(&bcMux);
  if (!automationPersistGlobal(enabled, flags)) { bcSendError(500, "save failed"); return; }
  portENTER_CRITICAL(&bcMux);
  autoEnabled = enabled;
  if (!enabled)
    for (uint8_t i = 0; i < AUTO_RULE_MAX; ++i)
      if (!autoRt[i].manual) autoRt[i].running = false;
  portEXIT_CRITICAL(&bcMux);
  server.send(200, "application/json", automationJson());
}

static void httpAutomationFlag() {
  uint32_t idx = 0;
  bool value = false, present = false, present2 = false;
  if (!bcParseUIntArg("idx", 0u, AUTO_FLAG_COUNT - 1u, idx, present) || !present ||
      !bcParseBoolArg("value", value, present2) || !present2) {
    bcSendError(400, "idx 0-7 and value=0|1 required");
    return;
  }
  bool enabled;
  uint8_t flags;
  portENTER_CRITICAL(&bcMux);
  enabled = autoEnabled;
  flags = value ? (uint8_t)(autoFlags | (1u << idx)) : (uint8_t)(autoFlags & ~(1u << idx));
  portEXIT_CRITICAL(&bcMux);
  if (!automationPersistGlobal(enabled, flags)) { bcSendError(500, "save failed"); return; }
  portENTER_CRITICAL(&bcMux);
  autoFlags = value ? (uint8_t)(autoFlags | (1u << idx)) : (uint8_t)(autoFlags & ~(1u << idx));
  autoGlobalDirty = false;
  portEXIT_CRITICAL(&bcMux);
  server.send(200, "application/json", automationJson());
}

// Runs a rule's actions now (no trigger, conditions or cooldown); dry-run and
// the drive-gear block list still apply.
static void httpAutomationRun() {
  uint32_t idx = 0;
  if (!autoParseIdx(idx)) { bcSendError(400, "idx 0-15 required"); return; }
  if (bodyComfortActiveBus() == BC_BUS_NONE) { bcSendError(409, "Automation requires Body CAN"); return; }
  const char *why = nullptr;
  portENTER_CRITICAL(&bcMux);
  if (!autoRules[idx].used) why = "empty rule";
  else if (autoRt[idx].running) why = "rule is running";
  else autoStartRunnerLocked((uint32_t)millis(), (uint8_t)idx, true);
  portEXIT_CRITICAL(&bcMux);
  if (why) { bcSendError(409, why); return; }
  server.send(200, "application/json", automationJson());
}
