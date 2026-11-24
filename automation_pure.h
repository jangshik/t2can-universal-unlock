#pragma once
// Automation (phase 1): user rules of the form
//   WHEN <trigger>  IF <conditions joined with AND / OR>  THEN <up to 3 actions>
// evaluated on the Body Comfort task. Pure logic only: signal catalog, rule
// validation, trigger edge detection, condition evaluation, the drive-gear
// action block list and the NVS blob format. Host-testable.
#include <stdint.h>
#include <string.h>

static constexpr uint8_t AUTO_RULE_MAX = 16u;
static constexpr uint8_t AUTO_COND_MAX = 4u;
static constexpr uint8_t AUTO_ACT_MAX = 3u;
static constexpr uint8_t AUTO_FLAG_COUNT = 8u;
static constexpr uint8_t AUTO_NAME_LEN = 32u;  // bytes incl. NUL (UTF-8)
static constexpr int16_t AUTO_ANY = -32768;    // "any value" in a CHANGE trigger
static constexpr uint32_t AUTO_STALE_RESET_MS = 3000u;
static constexpr uint16_t AUTO_COOLDOWN_MIN_S = 1u;
static constexpr uint16_t AUTO_COOLDOWN_MAX_S = 3600u;
static constexpr uint8_t AUTO_HOLD_MAX_S = 250u;

// ─── Signals ─────────────────────────────────────────────────────────────
// Values every signal reports. Booleans are 0 / 1.
enum AutoSignalPure : uint8_t {
  AUTO_SIG_NONE = 0,
  AUTO_SIG_GEAR = 1,          // 1 P, 2 R, 3 N, 4 D
  AUTO_SIG_SPEED = 2,         // km/h
  AUTO_SIG_PEDAL = 3,         // accelerator %
  AUTO_SIG_BRAKE = 4,         // 1 pressed
  AUTO_SIG_DOOR_FL = 5,       // 1 open (FL FR RL RR = 5..8)
  AUTO_SIG_DOOR_FR = 6,
  AUTO_SIG_DOOR_RL = 7,
  AUTO_SIG_DOOR_RR = 8,
  AUTO_SIG_ANY_DOOR = 9,
  AUTO_SIG_HANDLE_FL = 10,    // exterior handle pulled
  AUTO_SIG_HANDLE_FR = 11,
  AUTO_SIG_LOCKED = 12,       // 1 locked
  AUTO_SIG_WIN_FL = 13,       // 0 closed, 1 vent, 2 partly open, 3 fully open (FL..RR = 13..16)
  AUTO_SIG_WIN_FR = 14,
  AUTO_SIG_WIN_RL = 15,
  AUTO_SIG_WIN_RR = 16,
  AUTO_SIG_ANY_WIN_OPEN = 17, // any window not closed
  AUTO_SIG_RAIN = 18,
  AUTO_SIG_SNOW = 19,
  AUTO_SIG_RAIN_OR_SNOW = 20,
  AUTO_SIG_MIRRORS = 21,      // 0 unfolded, 1 folded, 2 moving
  AUTO_SIG_CAMERA = 22,       // camera view on screen
  AUTO_SIG_HAZARDS = 23,      // hazard lights on
  AUTO_SIG_LIGHT_SWITCH = 24, // 0 AUTO, 1 ON, 2 PARKING, 3 OFF
  AUTO_SIG_BELT_2L = 25,      // 2nd-row seat belt unbuckled (L C R = 25..27)
  AUTO_SIG_BELT_2C = 26,
  AUTO_SIG_BELT_2R = 27,
  AUTO_SIG_BASIC_END = 28,
  AUTO_SIG_FEATURE_BASE = 40, // + AutoFeaturePure: feature switched on
  AUTO_SIG_FLAG_BASE = 80,    // + 0..7: user flag on
};

enum AutoFeaturePure : uint8_t {
  AUTO_FEAT_EASY_ENTRY = 0,
  AUTO_FEAT_EE_MIRROR_FOLD = 1,
  AUTO_FEAT_SEATBELT = 2,
  AUTO_FEAT_REVERSE_HAZARD = 3,
  AUTO_FEAT_REVERSE_LIGHT = 4,
  AUTO_FEAT_DOOR_HAZARD = 5,
  AUTO_FEAT_KICKDOWN = 6,
  AUTO_FEAT_DOOR_HANDLE = 7,
  AUTO_FEAT_MACROS = 8,
  AUTO_FEAT_HAZARD_BLINK = 9,
  AUTO_FEAT_NAG_KILLER = 10,   // 10..19: pre-existing features
  AUTO_FEAT_ISA = 11,
  AUTO_FEAT_DMS = 12,
  AUTO_FEAT_AUTO_BLINKER = 13,
  AUTO_FEAT_TLSSC = 14,
  AUTO_FEAT_DOOR_CANCEL = 15,
  AUTO_FEAT_VISION = 16,
  AUTO_FEAT_CONT_AP = 17,
  AUTO_FEAT_PREHEAT = 18,
  AUTO_FEAT_LISTEN_ONLY = 19,
  AUTO_FEAT_COUNT = 20,
};
static constexpr uint8_t AUTO_FEAT_EXISTING_FIRST = AUTO_FEAT_NAG_KILLER;

enum AutoSignalKindPure : uint8_t { AUTO_KIND_BOOL = 0, AUTO_KIND_ENUM = 1, AUTO_KIND_NUMBER = 2 };

struct AutoSignalInfoPure {
  const char *key;
  uint8_t kind;
  int16_t min;
  int16_t max;
};

static inline bool autoSignalInfoPure(uint8_t sig, AutoSignalInfoPure &out) {
  static const AutoSignalInfoPure basic[AUTO_SIG_BASIC_END] = {
      {"none", AUTO_KIND_BOOL, 0, 0},
      {"gear", AUTO_KIND_ENUM, 1, 4},
      {"speed", AUTO_KIND_NUMBER, -50, 250},
      {"pedal", AUTO_KIND_NUMBER, 0, 100},
      {"brake", AUTO_KIND_BOOL, 0, 1},
      {"doorFL", AUTO_KIND_BOOL, 0, 1},
      {"doorFR", AUTO_KIND_BOOL, 0, 1},
      {"doorRL", AUTO_KIND_BOOL, 0, 1},
      {"doorRR", AUTO_KIND_BOOL, 0, 1},
      {"anyDoor", AUTO_KIND_BOOL, 0, 1},
      {"handleFL", AUTO_KIND_BOOL, 0, 1},
      {"handleFR", AUTO_KIND_BOOL, 0, 1},
      {"locked", AUTO_KIND_BOOL, 0, 1},
      {"winFL", AUTO_KIND_ENUM, 0, 3},
      {"winFR", AUTO_KIND_ENUM, 0, 3},
      {"winRL", AUTO_KIND_ENUM, 0, 3},
      {"winRR", AUTO_KIND_ENUM, 0, 3},
      {"anyWindowOpen", AUTO_KIND_BOOL, 0, 1},
      {"rain", AUTO_KIND_BOOL, 0, 1},
      {"snow", AUTO_KIND_BOOL, 0, 1},
      {"rainOrSnow", AUTO_KIND_BOOL, 0, 1},
      {"mirrors", AUTO_KIND_ENUM, 0, 2},
      {"camera", AUTO_KIND_BOOL, 0, 1},
      {"hazards", AUTO_KIND_BOOL, 0, 1},
      {"lightSwitch", AUTO_KIND_ENUM, 0, 3},
      {"belt2L", AUTO_KIND_BOOL, 0, 1},
      {"belt2C", AUTO_KIND_BOOL, 0, 1},
      {"belt2R", AUTO_KIND_BOOL, 0, 1},
  };
  static const char *const featureKeys[AUTO_FEAT_COUNT] = {
      "feature.easyEntry", "feature.eeMirrorFold", "feature.seatbelt", "feature.reverseHazard",
      "feature.reverseLight", "feature.doorHazard", "feature.kickdown", "feature.doorHandle",
      "feature.macros", "feature.hazardBlink", "feature.nagKiller", "feature.isa", "feature.dms",
      "feature.autoBlinker", "feature.tlssc", "feature.doorCancel", "feature.vision",
      "feature.contAp", "feature.preheat", "feature.listenOnly"};
  static const char *const flagKeys[AUTO_FLAG_COUNT] = {"flag1", "flag2", "flag3", "flag4",
                                                        "flag5", "flag6", "flag7", "flag8"};
  if (sig > AUTO_SIG_NONE && sig < AUTO_SIG_BASIC_END) { out = basic[sig]; return true; }
  if (sig >= AUTO_SIG_FEATURE_BASE && sig < AUTO_SIG_FEATURE_BASE + AUTO_FEAT_COUNT) {
    out = {featureKeys[sig - AUTO_SIG_FEATURE_BASE], AUTO_KIND_BOOL, 0, 1};
    return true;
  }
  if (sig >= AUTO_SIG_FLAG_BASE && sig < AUTO_SIG_FLAG_BASE + AUTO_FLAG_COUNT) {
    out = {flagKeys[sig - AUTO_SIG_FLAG_BASE], AUTO_KIND_BOOL, 0, 1};
    return true;
  }
  return false;
}

// Window position state (0x2C2/0x2C3 windowPositionState) → signal value.
static inline bool autoWindowValuePure(uint8_t posState, int16_t &out) {
  switch (posState) {
    case 1: case 6: out = 0; return true;  // CLOSED, CLOSED_TRIM_CLEAR
    case 2: case 3: out = 1; return true;  // CRACKED, VENT
    case 4: out = 2; return true;          // PARTIAL_OPEN
    case 5: out = 3; return true;          // FULL_OPEN
    default: return false;
  }
}

// ─── Comparison ──────────────────────────────────────────────────────────
enum AutoOpPure : uint8_t {
  AUTO_OP_EQ = 0, AUTO_OP_NE = 1, AUTO_OP_GT = 2, AUTO_OP_GE = 3, AUTO_OP_LT = 4, AUTO_OP_LE = 5,
  AUTO_OP_COUNT = 6
};

static inline bool autoOpAllowedPure(uint8_t kind, uint8_t op) {
  if (op >= AUTO_OP_COUNT) return false;
  return kind == AUTO_KIND_NUMBER || op == AUTO_OP_EQ || op == AUTO_OP_NE;
}

static inline bool autoComparePure(int16_t v, uint8_t op, int16_t ref) {
  switch (op) {
    case AUTO_OP_EQ: return v == ref;
    case AUTO_OP_NE: return v != ref;
    case AUTO_OP_GT: return v > ref;
    case AUTO_OP_GE: return v >= ref;
    case AUTO_OP_LT: return v < ref;
    case AUTO_OP_LE: return v <= ref;
    default: return false;
  }
}

// Snapshot of every signal for one evaluation pass.
struct AutoSignalsPure {
  int16_t value[AUTO_SIG_FLAG_BASE + AUTO_FLAG_COUNT];
  bool valid[AUTO_SIG_FLAG_BASE + AUTO_FLAG_COUNT];
};
static constexpr uint8_t AUTO_SIG_SLOTS = AUTO_SIG_FLAG_BASE + AUTO_FLAG_COUNT;

static inline void autoSignalSetPure(AutoSignalsPure &s, uint8_t sig, bool valid, int16_t v) {
  if (sig >= AUTO_SIG_SLOTS) return;
  s.valid[sig] = valid;
  s.value[sig] = valid ? v : 0;
}

// ─── Rule ────────────────────────────────────────────────────────────────
enum AutoTriggerTypePure : uint8_t {
  AUTO_TRIG_NONE = 0,
  AUTO_TRIG_STATE = 1,   // <signal op value> becomes true and stays true for holdS
  AUTO_TRIG_CHANGE = 2,  // signal changes from <from> to <value> (AUTO_ANY = any) and stays holdS
  AUTO_TRIG_COUNT = 3
};

struct AutoTriggerPure {
  uint8_t type;
  uint8_t signal;
  uint8_t op;      // STATE only
  int16_t value;   // STATE: compared value. CHANGE: new value or AUTO_ANY
  int16_t from;    // CHANGE: previous value or AUTO_ANY
  uint8_t holdS;
};

static constexpr uint8_t AUTO_COND_OR = 0x80u;  // in AutoConditionPure.opFlags: OR with previous

struct AutoConditionPure {
  uint8_t signal;   // AUTO_SIG_NONE = unused
  uint8_t opFlags;  // low bits AutoOpPure, bit 7 = OR (otherwise AND) with the previous one
  int16_t value;
};

enum AutoActionTypePure : uint8_t {
  AUTO_ACT_NONE = 0,
  AUTO_ACT_HAZARD_BLINK = 1,  // b = seconds 1..60
  AUTO_ACT_MIRRORS = 2,       // a = 0 unfold, 1 fold
  AUTO_ACT_WINDOWS = 3,       // a = window mask bit0 FL .. bit3 RR, b = 2 vent, 3 close, 4 open
  AUTO_ACT_CAMERA = 4,        // press the camera button once
  AUTO_ACT_FRUNK = 5,         // open the frunk (P only)
  AUTO_ACT_REAR_FOG = 6,      // a = 0 off, 1 on for b seconds (1..3600)
  AUTO_ACT_FEATURE = 7,       // a = AutoFeaturePure, b = 0 off, 1 on, 2 toggle
  AUTO_ACT_PEDAL_MODE = 8,    // a = 0 Chill, 1 Standard, 2 Sport, 3 vehicle setting
  AUTO_ACT_FLAG = 9,          // a = flag 0..7, b = 0 off, 1 on, 2 toggle
  AUTO_ACT_DELAY = 10,        // b = seconds 1..600 before the next action
  AUTO_ACT_COUNT = 11
};

struct AutoActionPure {
  uint8_t type;
  uint8_t a;
  uint16_t b;
};

struct AutoRulePure {
  bool used;
  bool enabled;
  bool dryRun;    // log what would run, send nothing
  bool restart;   // a new trigger restarts a running action list (otherwise ignored)
  uint16_t cooldownS;
  AutoTriggerPure trig;
  AutoConditionPure cond[AUTO_COND_MAX];
  AutoActionPure act[AUTO_ACT_MAX];
  char name[AUTO_NAME_LEN];
};

// ─── Drive-gear block list ───────────────────────────────────────────────
// Actions that never run while the car is in D (or when the gear is unknown).
// Add entries here to block more; AUTO_MATCH_ANY matches every parameter.
static constexpr uint8_t AUTO_MATCH_ANY = 0xFFu;
struct AutoDriveBlockPure { uint8_t type; uint8_t a; };
static constexpr AutoDriveBlockPure AUTO_DRIVE_BLOCKLIST[] = {
    {AUTO_ACT_MIRRORS, 1u},              // fold mirrors
    {AUTO_ACT_REAR_FOG, 1u},             // rear fog on
    {AUTO_ACT_FRUNK, AUTO_MATCH_ANY},    // open frunk
};
static constexpr uint8_t AUTO_DRIVE_BLOCK_COUNT =
    (uint8_t)(sizeof(AUTO_DRIVE_BLOCKLIST) / sizeof(AUTO_DRIVE_BLOCKLIST[0]));

static inline bool autoActionDriveBlockedPure(const AutoActionPure &a) {
  for (uint8_t i = 0; i < AUTO_DRIVE_BLOCK_COUNT; ++i)
    if (AUTO_DRIVE_BLOCKLIST[i].type == a.type &&
        (AUTO_DRIVE_BLOCKLIST[i].a == AUTO_MATCH_ANY || AUTO_DRIVE_BLOCKLIST[i].a == a.a))
      return true;
  return false;
}

// gearKnown false = no fresh gear: blocked actions are refused (fail-closed).
static inline bool autoActionAllowedInGearPure(const AutoActionPure &a, bool gearKnown, uint8_t gear) {
  if (!autoActionDriveBlockedPure(a)) return true;
  return gearKnown && gear != 4u;  // 4 = D
}

// Actions that send CAN frames (skipped in listen-only).
static inline bool autoActionSendsCanPure(uint8_t type) {
  switch (type) {
    case AUTO_ACT_HAZARD_BLINK: case AUTO_ACT_MIRRORS: case AUTO_ACT_WINDOWS:
    case AUTO_ACT_CAMERA: case AUTO_ACT_FRUNK: case AUTO_ACT_REAR_FOG: case AUTO_ACT_PEDAL_MODE:
      return true;
    default:
      return false;
  }
}

// ─── Validation ──────────────────────────────────────────────────────────
static inline bool autoValueInRangePure(uint8_t sig, int16_t v) {
  AutoSignalInfoPure info;
  return autoSignalInfoPure(sig, info) && v >= info.min && v <= info.max;
}

static inline const char *autoActionValidPure(const AutoActionPure &a) {
  switch (a.type) {
    case AUTO_ACT_NONE: return nullptr;
    case AUTO_ACT_HAZARD_BLINK: return (a.b >= 1u && a.b <= 60u) ? nullptr : "hazard seconds 1-60";
    case AUTO_ACT_MIRRORS: return a.a <= 1u ? nullptr : "mirror fold / unfold";
    case AUTO_ACT_WINDOWS:
      return (a.a >= 1u && a.a <= 15u && a.b >= 2u && a.b <= 4u) ? nullptr : "windows mask / request";
    case AUTO_ACT_CAMERA: case AUTO_ACT_FRUNK: return nullptr;
    case AUTO_ACT_REAR_FOG:
      return (a.a == 0u || (a.a == 1u && a.b >= 1u && a.b <= 3600u)) ? nullptr : "rear fog on 1-3600 s";
    case AUTO_ACT_FEATURE:
      return (a.a < AUTO_FEAT_COUNT && a.b <= 2u) ? nullptr : "feature / state";
    case AUTO_ACT_PEDAL_MODE: return a.a <= 3u ? nullptr : "pedal mode";
    case AUTO_ACT_FLAG: return (a.a < AUTO_FLAG_COUNT && a.b <= 2u) ? nullptr : "flag / state";
    case AUTO_ACT_DELAY: return (a.b >= 1u && a.b <= 600u) ? nullptr : "delay 1-600 s";
    default: return "unknown action";
  }
}

// Returns nullptr when the rule is valid, otherwise the first problem.
static inline const char *autoRuleValidPure(const AutoRulePure &r) {
  if (!r.used) return nullptr;
  if (r.cooldownS < AUTO_COOLDOWN_MIN_S || r.cooldownS > AUTO_COOLDOWN_MAX_S) return "cooldown 1-3600 s";
  AutoSignalInfoPure info;
  const AutoTriggerPure &t = r.trig;
  if (t.type == AUTO_TRIG_NONE || t.type >= AUTO_TRIG_COUNT) return "trigger type";
  if (!autoSignalInfoPure(t.signal, info)) return "trigger signal";
  if (t.holdS > AUTO_HOLD_MAX_S) return "hold 0-250 s";
  if (t.type == AUTO_TRIG_STATE) {
    if (!autoOpAllowedPure(info.kind, t.op)) return "trigger comparison";
    if (!autoValueInRangePure(t.signal, t.value)) return "trigger value";
  } else {
    if (t.value != AUTO_ANY && !autoValueInRangePure(t.signal, t.value)) return "trigger value";
    if (t.from != AUTO_ANY && !autoValueInRangePure(t.signal, t.from)) return "trigger from value";
    if (t.from != AUTO_ANY && t.value != AUTO_ANY && t.from == t.value) return "from and to are equal";
  }
  bool ended = false;
  for (uint8_t i = 0; i < AUTO_COND_MAX; ++i) {
    const AutoConditionPure &c = r.cond[i];
    if (c.signal == AUTO_SIG_NONE) { ended = true; continue; }
    if (ended) return "conditions must be contiguous";
    if (!autoSignalInfoPure(c.signal, info)) return "condition signal";
    if (!autoOpAllowedPure(info.kind, (uint8_t)(c.opFlags & 0x7Fu))) return "condition comparison";
    if ((c.opFlags & 0x78u) != 0u) return "condition comparison";
    if (!autoValueInRangePure(c.signal, c.value)) return "condition value";
  }
  bool hasAction = false;
  for (uint8_t i = 0; i < AUTO_ACT_MAX; ++i) {
    const char *why = autoActionValidPure(r.act[i]);
    if (why) return why;
    if (r.act[i].type != AUTO_ACT_NONE && r.act[i].type != AUTO_ACT_DELAY) hasAction = true;
  }
  if (!hasAction) return "at least one action";
  return nullptr;
}

// ─── Evaluation ──────────────────────────────────────────────────────────
// Conditions in written order; AND binds tighter than OR:
//   c1 AND c2 OR c3 AND c4  ==  (c1 AND c2) OR (c3 AND c4)
// A condition on a stale / missing signal is false. No conditions = true.
static inline bool autoConditionsPassPure(const AutoRulePure &r, const AutoSignalsPure &s) {
  bool any = false, group = true, has = false;
  for (uint8_t i = 0; i < AUTO_COND_MAX; ++i) {
    const AutoConditionPure &c = r.cond[i];
    if (c.signal == AUTO_SIG_NONE || c.signal >= AUTO_SIG_SLOTS) break;
    if (has && (c.opFlags & AUTO_COND_OR) != 0u) { any = any || group; group = true; }
    has = true;
    const bool ok = s.valid[c.signal] && autoComparePure(s.value[c.signal], (uint8_t)(c.opFlags & 0x7Fu), c.value);
    group = group && ok;
  }
  return !has || any || group;
}

struct AutoTriggerStatePure {
  bool known;          // last value is valid (or stale for less than AUTO_STALE_RESET_MS)
  int16_t last;
  uint32_t lastValidMs;
  bool armed;
  uint32_t armMs;
  int16_t armValue;
};

static inline bool autoMatchPure(int16_t want, int16_t v) { return want == AUTO_ANY || want == v; }

// One step of edge detection. Returns true once per qualifying edge after the
// hold time. A value that comes back after a long gap is not an edge.
static inline bool autoTriggerStepPure(AutoTriggerStatePure &st, const AutoTriggerPure &t,
                                       uint32_t now, bool valid, int16_t v) {
  if (!valid) {
    if (st.known && (uint32_t)(now - st.lastValidMs) > AUTO_STALE_RESET_MS) {
      st.known = false;
      st.armed = false;
    }
    return false;
  }
  const bool wasKnown = st.known;
  const int16_t prev = st.last;
  st.known = true;
  st.last = v;
  st.lastValidMs = now;
  if (t.type == AUTO_TRIG_STATE) {
    const bool pred = autoComparePure(v, t.op, t.value);
    if (!pred) { st.armed = false; return false; }
    if (wasKnown && !autoComparePure(prev, t.op, t.value) && !st.armed) {
      st.armed = true;
      st.armMs = now;
    }
  } else if (t.type == AUTO_TRIG_CHANGE) {
    if (st.armed && v != st.armValue) st.armed = false;
    if (wasKnown && prev != v && autoMatchPure(t.from, prev) && autoMatchPure(t.value, v)) {
      st.armed = true;
      st.armMs = now;
      st.armValue = v;
    }
  } else {
    return false;
  }
  if (st.armed && (uint32_t)(now - st.armMs) >= (uint32_t)t.holdS * 1000u) {
    st.armed = false;
    return true;
  }
  return false;
}

// ─── NVS blob ────────────────────────────────────────────────────────────
// Rules: [version][count] then per used rule [slot][71 bytes], then checksum.
static constexpr uint8_t AUTO_BLOB_VERSION = 1u;
static constexpr uint8_t AUTO_RULE_BYTES = 1u + 2u + 8u + AUTO_COND_MAX * 4u + AUTO_ACT_MAX * 4u + AUTO_NAME_LEN;
static constexpr uint16_t AUTO_BLOB_MAX = 2u + AUTO_RULE_MAX * (1u + AUTO_RULE_BYTES) + 1u;
static_assert(AUTO_RULE_BYTES == 71u, "rule layout");

static inline uint8_t autoChecksumPure(const uint8_t *b, uint16_t len) {
  uint8_t sum = 0x5Au;
  for (uint16_t i = 0; i + 1u < len; ++i) sum = (uint8_t)(sum + b[i]);
  return sum;
}

static inline void autoPut16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v & 0xFFu); p[1] = (uint8_t)(v >> 8); }
static inline uint16_t autoGet16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static inline void autoRuleEncodePure(const AutoRulePure &r, uint8_t *o) {
  o[0] = (uint8_t)((r.enabled ? 1u : 0u) | (r.dryRun ? 2u : 0u) | (r.restart ? 4u : 0u));
  autoPut16(o + 1, r.cooldownS);
  o[3] = r.trig.type; o[4] = r.trig.signal; o[5] = r.trig.op;
  autoPut16(o + 6, (uint16_t)r.trig.value); autoPut16(o + 8, (uint16_t)r.trig.from);
  o[10] = r.trig.holdS;
  uint8_t i = 11u;
  for (uint8_t c = 0; c < AUTO_COND_MAX; ++c) {
    o[i++] = r.cond[c].signal; o[i++] = r.cond[c].opFlags;
    autoPut16(o + i, (uint16_t)r.cond[c].value); i += 2u;
  }
  for (uint8_t a = 0; a < AUTO_ACT_MAX; ++a) {
    o[i++] = r.act[a].type; o[i++] = r.act[a].a;
    autoPut16(o + i, r.act[a].b); i += 2u;
  }
  memcpy(o + i, r.name, AUTO_NAME_LEN);
  o[i + AUTO_NAME_LEN - 1u] = 0;
}

static inline void autoRuleDecodePure(const uint8_t *o, AutoRulePure &r) {
  r = {};
  r.used = true;
  r.enabled = (o[0] & 1u) != 0u; r.dryRun = (o[0] & 2u) != 0u; r.restart = (o[0] & 4u) != 0u;
  r.cooldownS = autoGet16(o + 1);
  r.trig.type = o[3]; r.trig.signal = o[4]; r.trig.op = o[5];
  r.trig.value = (int16_t)autoGet16(o + 6); r.trig.from = (int16_t)autoGet16(o + 8);
  r.trig.holdS = o[10];
  uint8_t i = 11u;
  for (uint8_t c = 0; c < AUTO_COND_MAX; ++c) {
    r.cond[c].signal = o[i++]; r.cond[c].opFlags = o[i++];
    r.cond[c].value = (int16_t)autoGet16(o + i); i += 2u;
  }
  for (uint8_t a = 0; a < AUTO_ACT_MAX; ++a) {
    r.act[a].type = o[i++]; r.act[a].a = o[i++];
    r.act[a].b = autoGet16(o + i); i += 2u;
  }
  memcpy(r.name, o + i, AUTO_NAME_LEN);
  r.name[AUTO_NAME_LEN - 1u] = 0;
}

// Returns the blob length.
static inline uint16_t autoSerializePure(const AutoRulePure *rules, uint8_t *out) {
  out[0] = AUTO_BLOB_VERSION;
  uint8_t count = 0;
  uint16_t i = 2u;
  for (uint8_t s = 0; s < AUTO_RULE_MAX; ++s) {
    if (!rules[s].used) continue;
    out[i++] = s;
    autoRuleEncodePure(rules[s], out + i);
    i += AUTO_RULE_BYTES;
    count++;
  }
  out[1] = count;
  const uint16_t len = (uint16_t)(i + 1u);
  out[i] = autoChecksumPure(out, len);
  return len;
}

// Invalid rules in an otherwise intact blob are dropped one by one.
static inline bool autoDeserializePure(const uint8_t *in, uint16_t len, AutoRulePure *rules) {
  for (uint8_t s = 0; s < AUTO_RULE_MAX; ++s) rules[s] = {};
  if (in == nullptr || len < 3u || in[0] != AUTO_BLOB_VERSION) return false;
  const uint8_t count = in[1];
  if (count > AUTO_RULE_MAX || len != (uint16_t)(2u + count * (1u + AUTO_RULE_BYTES) + 1u)) return false;
  if (autoChecksumPure(in, len) != in[len - 1u]) return false;
  uint16_t i = 2u;
  for (uint8_t n = 0; n < count; ++n) {
    const uint8_t slot = in[i++];
    AutoRulePure r;
    autoRuleDecodePure(in + i, r);
    i += AUTO_RULE_BYTES;
    if (slot < AUTO_RULE_MAX && !rules[slot].used && autoRuleValidPure(r) == nullptr) rules[slot] = r;
  }
  return true;
}

// Global state: [version][enabled][flags][checksum].
static constexpr uint8_t AUTO_GLOBAL_BYTES = 4u;
static inline void autoGlobalEncodePure(bool enabled, uint8_t flags, uint8_t out[AUTO_GLOBAL_BYTES]) {
  out[0] = 1u; out[1] = enabled ? 1u : 0u; out[2] = flags;
  out[3] = autoChecksumPure(out, AUTO_GLOBAL_BYTES);
}
static inline bool autoGlobalDecodePure(const uint8_t *in, uint16_t len, bool &enabled, uint8_t &flags) {
  enabled = false; flags = 0;
  if (in == nullptr || len != AUTO_GLOBAL_BYTES || in[0] != 1u || in[1] > 1u ||
      autoChecksumPure(in, AUTO_GLOBAL_BYTES) != in[3]) return false;
  enabled = in[1] == 1u; flags = in[2];
  return true;
}

// Sanitize a user-supplied name: printable UTF-8 bytes, no quotes, backslashes
// or control characters; cut on a character boundary.
static inline void autoNameSanitizePure(const char *in, char out[AUTO_NAME_LEN]) {
  memset(out, 0, AUTO_NAME_LEN);
  if (in == nullptr) return;
  uint8_t o = 0;
  for (const uint8_t *p = (const uint8_t *)in; *p;) {
    uint8_t n = 1;
    if (*p >= 0xF0u) n = 4; else if (*p >= 0xE0u) n = 3; else if (*p >= 0xC0u) n = 2;
    else if (*p >= 0x80u) { ++p; continue; }  // stray continuation byte
    bool ok = true;
    for (uint8_t k = 1; k < n; ++k) if ((p[k] & 0xC0u) != 0x80u) ok = false;
    if (!ok) { ++p; continue; }
    if (n == 1 && (*p < 0x20u || *p == '"' || *p == '\\' || *p == 0x7Fu)) { ++p; continue; }
    if ((uint16_t)o + n > AUTO_NAME_LEN - 1u) break;
    for (uint8_t k = 0; k < n; ++k) out[o++] = (char)p[k];
    p += n;
  }
}
