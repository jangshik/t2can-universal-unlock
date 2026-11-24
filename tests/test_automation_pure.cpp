// Host tests for automation_pure.h. Build:
//   g++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_automation_pure.cpp
#include <cassert>
#include <cstdio>
#include <cstring>
#include "automation_pure.h"

static AutoRulePure baseRule() {
  AutoRulePure r = {};
  r.used = true;
  r.enabled = true;
  r.cooldownS = 10;
  r.trig = {AUTO_TRIG_STATE, AUTO_SIG_RAIN, AUTO_OP_EQ, 1, AUTO_ANY, 0};
  r.act[0] = {AUTO_ACT_WINDOWS, 0x0F, 3};
  strcpy(r.name, "Rain close");
  return r;
}

static void testCatalog() {
  AutoSignalInfoPure i;
  assert(!autoSignalInfoPure(AUTO_SIG_NONE, i));
  assert(autoSignalInfoPure(AUTO_SIG_GEAR, i) && i.kind == AUTO_KIND_ENUM && i.min == 1 && i.max == 4);
  assert(autoSignalInfoPure(AUTO_SIG_SPEED, i) && i.kind == AUTO_KIND_NUMBER);
  assert(autoSignalInfoPure(AUTO_SIG_BELT_2R, i) && strcmp(i.key, "belt2R") == 0);
  assert(!autoSignalInfoPure(AUTO_SIG_BASIC_END, i));
  assert(!autoSignalInfoPure(39, i));
  assert(autoSignalInfoPure(AUTO_SIG_FEATURE_BASE + AUTO_FEAT_LISTEN_ONLY, i) &&
         strcmp(i.key, "feature.listenOnly") == 0);
  assert(!autoSignalInfoPure(AUTO_SIG_FEATURE_BASE + AUTO_FEAT_COUNT, i));
  assert(autoSignalInfoPure(AUTO_SIG_FLAG_BASE + 7, i) && strcmp(i.key, "flag8") == 0);
  assert(!autoSignalInfoPure(AUTO_SIG_FLAG_BASE + 8, i));
  int16_t v = -1;
  assert(autoWindowValuePure(1, v) && v == 0);
  assert(autoWindowValuePure(6, v) && v == 0);
  assert(autoWindowValuePure(2, v) && v == 1);
  assert(autoWindowValuePure(3, v) && v == 1);
  assert(autoWindowValuePure(4, v) && v == 2);
  assert(autoWindowValuePure(5, v) && v == 3);
  assert(!autoWindowValuePure(0, v));
  assert(autoOpAllowedPure(AUTO_KIND_NUMBER, AUTO_OP_GT));
  assert(!autoOpAllowedPure(AUTO_KIND_BOOL, AUTO_OP_GT));
  assert(autoOpAllowedPure(AUTO_KIND_ENUM, AUTO_OP_NE));
  assert(!autoOpAllowedPure(AUTO_KIND_NUMBER, AUTO_OP_COUNT));
}

static void testValidation() {
  AutoRulePure r = baseRule();
  assert(autoRuleValidPure(r) == nullptr);
  r.cooldownS = 0; assert(autoRuleValidPure(r) != nullptr); r.cooldownS = 10;
  r.trig.op = AUTO_OP_GT; assert(autoRuleValidPure(r) != nullptr); r.trig.op = AUTO_OP_EQ;
  r.trig.value = 2; assert(autoRuleValidPure(r) != nullptr); r.trig.value = 1;
  r.trig.signal = 30; assert(autoRuleValidPure(r) != nullptr); r.trig.signal = AUTO_SIG_RAIN;
  r.trig.holdS = 251; assert(autoRuleValidPure(r) != nullptr); r.trig.holdS = 0;
  // CHANGE: from / to may be ANY, but not equal.
  r.trig = {AUTO_TRIG_CHANGE, AUTO_SIG_GEAR, 0, 1, 2, 0};
  assert(autoRuleValidPure(r) == nullptr);
  r.trig.from = 1; assert(autoRuleValidPure(r) != nullptr);
  r.trig.from = AUTO_ANY; r.trig.value = AUTO_ANY; assert(autoRuleValidPure(r) == nullptr);
  r.trig.from = 9; assert(autoRuleValidPure(r) != nullptr);
  r = baseRule();
  // Conditions: contiguous, valid comparisons and ranges.
  r.cond[0] = {AUTO_SIG_GEAR, AUTO_OP_EQ, 1};
  r.cond[1] = {AUTO_SIG_SPEED, (uint8_t)(AUTO_OP_LT | AUTO_COND_OR), 5};
  assert(autoRuleValidPure(r) == nullptr);
  r.cond[3] = {AUTO_SIG_LOCKED, AUTO_OP_EQ, 1};
  assert(autoRuleValidPure(r) != nullptr);  // gap at index 2
  r.cond[3] = {};
  r.cond[1].value = 300; assert(autoRuleValidPure(r) != nullptr); r.cond[1].value = 5;
  r.cond[0].opFlags = AUTO_OP_GT; assert(autoRuleValidPure(r) != nullptr); r.cond[0].opFlags = AUTO_OP_EQ;
  r.cond[0].opFlags = 0x08; assert(autoRuleValidPure(r) != nullptr); r.cond[0].opFlags = AUTO_OP_EQ;
  // Actions.
  r.act[0] = {AUTO_ACT_DELAY, 0, 5};
  assert(autoRuleValidPure(r) != nullptr);  // only a delay
  r.act[1] = {AUTO_ACT_MIRRORS, 1, 0};
  assert(autoRuleValidPure(r) == nullptr);
  r.act[1] = {AUTO_ACT_MIRRORS, 2, 0}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_WINDOWS, 0, 3}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_WINDOWS, 16, 3}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_WINDOWS, 3, 1}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_REAR_FOG, 1, 0}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_REAR_FOG, 1, 30}; assert(autoRuleValidPure(r) == nullptr);
  r.act[1] = {AUTO_ACT_REAR_FOG, 0, 0}; assert(autoRuleValidPure(r) == nullptr);
  r.act[1] = {AUTO_ACT_FEATURE, AUTO_FEAT_COUNT, 1}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_FEATURE, AUTO_FEAT_TLSSC, 3}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_FEATURE, AUTO_FEAT_TLSSC, 2}; assert(autoRuleValidPure(r) == nullptr);
  r.act[1] = {AUTO_ACT_PEDAL_MODE, 4, 0}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_FLAG, 8, 1}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_HAZARD_BLINK, 0, 61}; assert(autoRuleValidPure(r) != nullptr);
  r.act[1] = {AUTO_ACT_COUNT, 0, 0}; assert(autoRuleValidPure(r) != nullptr);
  AutoRulePure unused = {};
  assert(autoRuleValidPure(unused) == nullptr);
}

static void testConditions() {
  AutoSignalsPure s = {};
  autoSignalSetPure(s, AUTO_SIG_GEAR, true, 1);
  autoSignalSetPure(s, AUTO_SIG_LOCKED, true, 0);
  autoSignalSetPure(s, AUTO_SIG_SPEED, true, 0);
  autoSignalSetPure(s, AUTO_SIG_RAIN, false, 0);
  AutoRulePure r = baseRule();
  assert(autoConditionsPassPure(r, s));  // none
  r.cond[0] = {AUTO_SIG_GEAR, AUTO_OP_EQ, 1};
  r.cond[1] = {AUTO_SIG_LOCKED, AUTO_OP_EQ, 1};
  assert(!autoConditionsPassPure(r, s));  // P AND locked
  r.cond[1].opFlags |= AUTO_COND_OR;
  assert(autoConditionsPassPure(r, s));   // P OR locked
  // (gear=D AND speed<5) OR (locked AND rain): both groups false.
  r.cond[0] = {AUTO_SIG_GEAR, AUTO_OP_EQ, 4};
  r.cond[1] = {AUTO_SIG_SPEED, AUTO_OP_LT, 5};
  r.cond[2] = {AUTO_SIG_LOCKED, (uint8_t)(AUTO_OP_EQ | AUTO_COND_OR), 1};
  r.cond[3] = {AUTO_SIG_RAIN, AUTO_OP_EQ, 1};
  assert(!autoConditionsPassPure(r, s));
  autoSignalSetPure(s, AUTO_SIG_GEAR, true, 4);
  assert(autoConditionsPassPure(r, s));   // first group
  autoSignalSetPure(s, AUTO_SIG_SPEED, true, 50);
  assert(!autoConditionsPassPure(r, s));
  autoSignalSetPure(s, AUTO_SIG_LOCKED, true, 1);
  assert(!autoConditionsPassPure(r, s));  // rain is stale: false
  autoSignalSetPure(s, AUTO_SIG_RAIN, true, 1);
  assert(autoConditionsPassPure(r, s));   // second group
  // A stale signal never matches, even with "not equal".
  r = baseRule();
  r.cond[0] = {AUTO_SIG_SNOW, AUTO_OP_NE, 1};
  assert(!autoConditionsPassPure(r, s));
  // Leading OR flag on the first condition is ignored.
  r.cond[0] = {AUTO_SIG_GEAR, (uint8_t)(AUTO_OP_EQ | AUTO_COND_OR), 4};
  assert(autoConditionsPassPure(r, s));
}

static void testStateTrigger() {
  AutoTriggerPure t = {AUTO_TRIG_STATE, AUTO_SIG_SPEED, AUTO_OP_GT, 80, AUTO_ANY, 5};
  AutoTriggerStatePure st = {};
  uint32_t now = 1000;
  // Already true when first seen: not an edge.
  assert(!autoTriggerStepPure(st, t, now, true, 100));
  for (int i = 0; i < 100; ++i) assert(!autoTriggerStepPure(st, t, now += 100, true, 100));
  assert(!autoTriggerStepPure(st, t, now += 100, true, 50));
  assert(!autoTriggerStepPure(st, t, now += 100, true, 90));  // edge, hold starts
  for (int i = 0; i < 49; ++i) assert(!autoTriggerStepPure(st, t, now += 100, true, 90));
  assert(autoTriggerStepPure(st, t, now += 100, true, 95));   // 5 s
  for (int i = 0; i < 100; ++i) assert(!autoTriggerStepPure(st, t, now += 100, true, 95));  // once
  // Dropping below during the hold cancels.
  assert(!autoTriggerStepPure(st, t, now += 100, true, 10));
  assert(!autoTriggerStepPure(st, t, now += 100, true, 90));
  assert(!autoTriggerStepPure(st, t, now += 2000, true, 90));
  assert(!autoTriggerStepPure(st, t, now += 100, true, 70));
  assert(!autoTriggerStepPure(st, t, now += 5000, true, 90));
  // Short stale gap keeps the last value; the hold still completes.
  assert(!autoTriggerStepPure(st, t, now += 1000, false, 0));
  assert(autoTriggerStepPure(st, t, now += 4000, true, 90));
  // Long gap forgets: value returning true is not an edge.
  assert(!autoTriggerStepPure(st, t, now += 100, true, 10));
  assert(!autoTriggerStepPure(st, t, now += 100, false, 0));
  assert(!autoTriggerStepPure(st, t, now += 3500, false, 0));
  assert(!autoTriggerStepPure(st, t, now += 100, true, 90));
  assert(!autoTriggerStepPure(st, t, now += 6000, true, 90));
  // Hold 0 fires on the edge itself.
  AutoTriggerPure t0 = {AUTO_TRIG_STATE, AUTO_SIG_DOOR_FL, AUTO_OP_EQ, 1, AUTO_ANY, 0};
  AutoTriggerStatePure s0 = {};
  assert(!autoTriggerStepPure(s0, t0, 100, true, 0));
  assert(autoTriggerStepPure(s0, t0, 200, true, 1));
  assert(!autoTriggerStepPure(s0, t0, 300, true, 1));
}

static void testChangeTrigger() {
  // Gear R -> P.
  AutoTriggerPure t = {AUTO_TRIG_CHANGE, AUTO_SIG_GEAR, 0, 1, 2, 0};
  AutoTriggerStatePure st = {};
  uint32_t now = 0;
  assert(!autoTriggerStepPure(st, t, now += 100, true, 1));
  assert(!autoTriggerStepPure(st, t, now += 100, true, 4));  // P -> D
  assert(!autoTriggerStepPure(st, t, now += 100, true, 1));  // D -> P: wrong from
  assert(!autoTriggerStepPure(st, t, now += 100, true, 2));
  assert(autoTriggerStepPure(st, t, now += 100, true, 1));   // R -> P
  assert(!autoTriggerStepPure(st, t, now += 100, true, 1));
  // Any -> D with 2 s hold, interrupted then held.
  AutoTriggerPure d = {AUTO_TRIG_CHANGE, AUTO_SIG_GEAR, 0, 4, AUTO_ANY, 2};
  AutoTriggerStatePure sd = {};
  assert(!autoTriggerStepPure(sd, d, now += 100, true, 1));
  assert(!autoTriggerStepPure(sd, d, now += 100, true, 4));
  assert(!autoTriggerStepPure(sd, d, now += 1000, true, 4));
  assert(!autoTriggerStepPure(sd, d, now += 100, true, 3));  // left D
  assert(!autoTriggerStepPure(sd, d, now += 100, true, 4));
  assert(!autoTriggerStepPure(sd, d, now += 1500, true, 4));
  assert(autoTriggerStepPure(sd, d, now += 500, true, 4));
  // Any change at all.
  AutoTriggerPure any = {AUTO_TRIG_CHANGE, AUTO_SIG_MIRRORS, 0, AUTO_ANY, AUTO_ANY, 0};
  AutoTriggerStatePure sa = {};
  assert(!autoTriggerStepPure(sa, any, 100, true, 0));
  assert(autoTriggerStepPure(sa, any, 200, true, 2));
  assert(autoTriggerStepPure(sa, any, 300, true, 1));
  assert(!autoTriggerStepPure(sa, any, 400, true, 1));
}

static void testDriveBlock() {
  AutoActionPure fold = {AUTO_ACT_MIRRORS, 1, 0}, unfold = {AUTO_ACT_MIRRORS, 0, 0};
  AutoActionPure fogOn = {AUTO_ACT_REAR_FOG, 1, 30}, fogOff = {AUTO_ACT_REAR_FOG, 0, 0};
  AutoActionPure frunk = {AUTO_ACT_FRUNK, 0, 0}, win = {AUTO_ACT_WINDOWS, 15, 3};
  assert(autoActionDriveBlockedPure(fold) && !autoActionDriveBlockedPure(unfold));
  assert(autoActionDriveBlockedPure(fogOn) && !autoActionDriveBlockedPure(fogOff));
  assert(autoActionDriveBlockedPure(frunk) && !autoActionDriveBlockedPure(win));
  assert(!autoActionAllowedInGearPure(fold, true, 4));
  assert(autoActionAllowedInGearPure(fold, true, 1));
  assert(autoActionAllowedInGearPure(fold, true, 2));
  assert(!autoActionAllowedInGearPure(fold, false, 1));  // unknown gear: fail closed
  assert(autoActionAllowedInGearPure(win, false, 0));
  assert(autoActionAllowedInGearPure(unfold, true, 4));
  assert(autoActionSendsCanPure(AUTO_ACT_WINDOWS) && !autoActionSendsCanPure(AUTO_ACT_FLAG));
  assert(!autoActionSendsCanPure(AUTO_ACT_FEATURE) && !autoActionSendsCanPure(AUTO_ACT_DELAY));
}

static void testBlob() {
  AutoRulePure rules[AUTO_RULE_MAX] = {};
  rules[0] = baseRule();
  rules[5] = baseRule();
  rules[5].dryRun = true; rules[5].restart = true; rules[5].enabled = false;
  rules[5].cooldownS = 3600;
  rules[5].trig = {AUTO_TRIG_CHANGE, AUTO_SIG_SPEED, 0, -20, AUTO_ANY, 250};
  rules[5].cond[0] = {AUTO_SIG_FLAG_BASE + 2, AUTO_OP_EQ, 1};
  rules[5].cond[1] = {AUTO_SIG_SPEED, (uint8_t)(AUTO_OP_LE | AUTO_COND_OR), -5};
  rules[5].act[1] = {AUTO_ACT_DELAY, 0, 600};
  rules[5].act[2] = {AUTO_ACT_FEATURE, AUTO_FEAT_PREHEAT, 2};
  strcpy(rules[5].name, "\xEC\x84\xB8\xEC\xB0\xA8 mode");  // "세차 mode"
  uint8_t blob[AUTO_BLOB_MAX];
  const uint16_t len = autoSerializePure(rules, blob);
  assert(len == 2u + 2u * 72u + 1u);
  AutoRulePure back[AUTO_RULE_MAX];
  assert(autoDeserializePure(blob, len, back));
  assert(back[0].used && back[5].used && !back[1].used);
  assert(memcmp(&back[5].trig, &rules[5].trig, sizeof(AutoTriggerPure)) == 0 ||
         (back[5].trig.type == rules[5].trig.type && back[5].trig.value == -20 &&
          back[5].trig.from == AUTO_ANY && back[5].trig.holdS == 250));
  assert(back[5].dryRun && back[5].restart && !back[5].enabled && back[5].cooldownS == 3600);
  assert(back[5].cond[1].value == -5 && (back[5].cond[1].opFlags & AUTO_COND_OR));
  assert(back[5].act[1].b == 600 && back[5].act[2].a == AUTO_FEAT_PREHEAT);
  assert(strcmp(back[5].name, rules[5].name) == 0);
  // Corruption.
  blob[10] ^= 1;
  assert(!autoDeserializePure(blob, len, back) && !back[0].used);
  blob[10] ^= 1;
  assert(!autoDeserializePure(blob, (uint16_t)(len - 1), back));
  // An invalid rule inside an intact blob is dropped alone.
  rules[0].cooldownS = 0;
  const uint16_t len2 = autoSerializePure(rules, blob);
  assert(autoDeserializePure(blob, len2, back) && !back[0].used && back[5].used);
  // Empty set.
  AutoRulePure none[AUTO_RULE_MAX] = {};
  const uint16_t len3 = autoSerializePure(none, blob);
  assert(len3 == 3u && autoDeserializePure(blob, len3, back));
  // Global.
  uint8_t g[AUTO_GLOBAL_BYTES];
  bool en = false; uint8_t fl = 0;
  autoGlobalEncodePure(true, 0x81, g);
  assert(autoGlobalDecodePure(g, AUTO_GLOBAL_BYTES, en, fl) && en && fl == 0x81);
  g[2] ^= 1;
  assert(!autoGlobalDecodePure(g, AUTO_GLOBAL_BYTES, en, fl) && !en && fl == 0);
}

static void testName() {
  char out[AUTO_NAME_LEN];
  autoNameSanitizePure("Rain \"close\" \\ all\x01", out);
  assert(strcmp(out, "Rain close  all") == 0);
  // 11 Korean syllables (33 bytes): cut on a character boundary at 30 bytes.
  autoNameSanitizePure("\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80"
                       "\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80", out);
  assert(strlen(out) == 30u);
  autoNameSanitizePure("\xEA\xB0" "ab\x80" "z", out);  // broken sequence dropped
  assert(strcmp(out, "abz") == 0);
  autoNameSanitizePure(nullptr, out);
  assert(out[0] == 0);
}

int main() {
  testCatalog();
  testValidation();
  testConditions();
  testStateTrigger();
  testChangeTrigger();
  testDriveBlock();
  testBlob();
  testName();
  std::puts("automation_pure: all tests passed");
  return 0;
}
