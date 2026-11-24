// Host tests for automation_runtime.h on the simulated Juniper Body CAN.
// Build:
//   g++ -std=c++17 -Wall -Wextra -Werror -I. -Itests tests/test_automation_runtime_host.cpp
#include "body_comfort_host_harness.h"

static AutoRulePure rule(uint8_t trigType, uint8_t sig, uint8_t op, int16_t value, int16_t from = AUTO_ANY,
                         uint8_t hold = 0) {
  AutoRulePure r = {};
  r.used = true;
  r.enabled = true;
  r.cooldownS = 1;
  r.trig = {trigType, sig, op, value, from, hold};
  return r;
}

static void put(uint8_t idx, const AutoRulePure &r) {
  assert(autoRuleValidPure(r) == nullptr);
  autoRules[idx] = r;
  autoRt[idx] = {};
}

static bool logHas(const char *result, uint8_t type = 0xFDu) {
  for (uint8_t k = 0; k < autoLogCount; ++k) {
    const AutoLogEntryRuntime &e = autoLog[(autoLogHead + k) % AUTO_LOG_LEN];
    if (e.result && strcmp(e.result, result) == 0 && (type == 0xFDu || e.type == type)) return true;
  }
  return false;
}

static std::vector<TxRecord> windowCommands(uint8_t request) {
  std::vector<TxRecord> out;
  for (const auto &r : takeTx(0x294))
    if (bcGetBitsPure(r.data, 52, 3) == request) out.push_back(r);
  return out;
}

static void testRainClosesWindows() {
  resetAll();
  autoEnabled = true;
  AutoRulePure r = rule(AUTO_TRIG_STATE, AUTO_SIG_RAIN_OR_SNOW, AUTO_OP_EQ, 1);
  r.cond[0] = {AUTO_SIG_GEAR, AUTO_OP_EQ, BC_GEAR_P};
  r.cond[1] = {AUTO_SIG_LOCKED, AUTO_OP_EQ, 1};
  r.act[0] = {AUTO_ACT_WINDOWS, 0x0F, BC_WINREQ_CLOSED};
  put(0, r);
  car.lockSimple = 2;
  for (int i = 0; i < 4; ++i) car.winPosState[i] = BC_WINPOS_VENT;
  run(1500);
  assert(windowCommands(BC_WINREQ_CLOSED).empty());
  car.rainy = true;
  run(1000);
  auto w = windowCommands(BC_WINREQ_CLOSED);
  assert(!w.empty());
  for (uint8_t d = 0; d < 4; ++d) assert(bcGetBitsPure(w[0].data, bcWindowRequestedBitPure(d), 1) == 1u);
  assert(autoRt[0].fires == 1 && logHas("triggered", AUTO_LOG_TRIGGERED) && logHas("OK", AUTO_ACT_WINDOWS));
  // Still raining: no second fire.
  g_tx.clear();
  run(5000);
  assert(windowCommands(BC_WINREQ_CLOSED).empty() && autoRt[0].fires == 1);

  // Unlocked: conditions fail, nothing is sent.
  resetAll();
  autoEnabled = true;
  put(0, r);
  run(1500);
  car.rainy = true;
  run(1000);
  assert(windowCommands(BC_WINREQ_CLOSED).empty());
  assert(autoRt[0].fires == 0 && autoRt[0].skippedCond == 1);
  // P OR locked: passes while unlocked.
  autoRules[0].cond[1].opFlags |= AUTO_COND_OR;
  car.rainy = false; run(500); car.rainy = true; run(1000);
  assert(!windowCommands(BC_WINREQ_CLOSED).empty() && autoRt[0].fires == 1);

  // Master switch off: nothing fires; switching it on later does not replay the edge.
  resetAll();
  put(0, r);
  car.lockSimple = 2;
  run(1500);
  car.rainy = true;
  run(1000);
  autoEnabled = true;
  run(2000);
  assert(windowCommands(BC_WINREQ_CLOSED).empty() && autoRt[0].fires == 0);
}

static void testDelayCooldownRestart() {
  resetAll();
  autoEnabled = true;
  AutoRulePure r = rule(AUTO_TRIG_STATE, AUTO_SIG_DOOR_FL, AUTO_OP_EQ, 1);
  r.cooldownS = 10;
  r.act[0] = {AUTO_ACT_FLAG, 0, 1};
  r.act[1] = {AUTO_ACT_DELAY, 0, 2};
  r.act[2] = {AUTO_ACT_FLAG, 0, 0};
  put(1, r);
  run(1000);
  car.latch[BC_DOOR_FL] = 1;
  run(300);
  assert((autoFlags & 1u) && autoRt[1].running);
  run(1500);
  assert((autoFlags & 1u));
  run(700);
  assert(!(autoFlags & 1u) && !autoRt[1].running);
  // Within the cooldown: skipped.
  car.latch[BC_DOOR_FL] = 2; run(300);
  car.latch[BC_DOOR_FL] = 1; run(300);
  assert(autoRt[1].fires == 1 && autoRt[1].skippedCooldown == 1 && !(autoFlags & 1u));
  // After the cooldown it fires again.
  car.latch[BC_DOOR_FL] = 2; run(8000);
  car.latch[BC_DOOR_FL] = 1; run(300);
  assert(autoRt[1].fires == 2 && (autoFlags & 1u));
  // Flags persist (debounced).
  run(2500);
  bool en = false; uint8_t fl = 0;
  const auto &g = g_nvs["bcauto/glob"];
  assert(autoGlobalDecodePure(g.data(), (uint16_t)g.size(), en, fl) && en && fl == autoFlags);

  // Single mode: a new trigger while running is ignored; restart mode starts over.
  resetAll();
  autoEnabled = true;
  r.cooldownS = 1;
  r.act[1].b = 5;
  put(1, r);
  run(1000);
  car.latch[BC_DOOR_FL] = 1; run(300);
  car.latch[BC_DOOR_FL] = 2; run(1500);
  car.latch[BC_DOOR_FL] = 1; run(300);
  assert(autoRt[1].fires == 1 && autoRt[1].skippedBusy == 1);
  run(4000);
  assert(!(autoFlags & 1u) && !autoRt[1].running);
}

static void testDriveBlockList() {
  resetAll();
  autoEnabled = true;
  AutoRulePure r = rule(AUTO_TRIG_STATE, AUTO_SIG_SPEED, AUTO_OP_GT, 30);
  r.act[0] = {AUTO_ACT_MIRRORS, 1, 0};
  r.act[1] = {AUTO_ACT_REAR_FOG, 1, 30};
  r.act[2] = {AUTO_ACT_FRUNK, 0, 0};
  put(2, r);
  car.gear = BC_GEAR_D;
  run(1000);
  car.kph10 = 400;
  run(1000);
  assert(takeTx(0x273).empty());
  assert(autoDriveBlocked == 3 && logHas("blocked in D", AUTO_ACT_MIRRORS) &&
         logHas("blocked in D", AUTO_ACT_REAR_FOG) && logHas("blocked in D", AUTO_ACT_FRUNK));
  assert(!bcState.autoFogActive && !bcState.mirrorFoldedByUs);
  // Unfolding is allowed in D.
  AutoRulePure u = rule(AUTO_TRIG_STATE, AUTO_SIG_SPEED, AUTO_OP_GT, 50);
  u.act[0] = {AUTO_ACT_MIRRORS, 0, 0};
  put(3, u);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  car.kph10 = 600;
  run(1000);
  auto m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_PRESENT);

  // Same rule in N: fold and rear fog run; the frunk keeps its own P-only gate.
  resetAll();
  autoEnabled = true;
  put(2, r);
  car.gear = BC_GEAR_N;
  run(1000);
  car.kph10 = 400;
  run(1000);
  m = takeTx(0x273);
  bool fold = false, fog = false;
  for (const auto &f : m) {
    if (bcGetBitsPure(f.data, 24, 2) == BC_MIRROR_REQ_RETRACT) fold = true;
    if (bcGetBitsPure(f.data, 23, 1) == 1u) fog = true;
  }
  assert(fold && fog && bcState.autoFogActive);
  assert(logHas("frunk only opens in P", AUTO_ACT_FRUNK));
  // Shifting into D ends the automation rear fog.
  car.gear = BC_GEAR_D;
  run(200);
  assert(!bcState.autoFogActive);
  g_tx.clear();
  run(500);
  for (const auto &f : takeTx(0x273)) assert(bcGetBitsPure(f.data, 23, 1) == 0u);

  // Rear fog times out on its own.
  resetAll();
  autoEnabled = true;
  AutoRulePure f = rule(AUTO_TRIG_STATE, AUTO_SIG_FLAG_BASE + 3, AUTO_OP_EQ, 1);
  f.act[0] = {AUTO_ACT_REAR_FOG, 1, 2};
  put(4, f);
  car.gear = BC_GEAR_R;
  run(1000);
  autoFlags = 0x08;
  run(300);
  assert(bcState.autoFogActive);
  run(2000);
  assert(!bcState.autoFogActive);
  // Unknown gear: blocked actions refused (fail-closed).
  resetAll();
  autoEnabled = true;
  put(4, f);
  run(1000);
  car.gear = 0;  // INVALID: gear goes stale
  run(2000);
  autoFlags = 0x08;
  run(300);
  assert(!bcState.autoFogActive && logHas("blocked in D", AUTO_ACT_REAR_FOG));
}

static void testDryRunListenOnlyAndBusy() {
  resetAll();
  autoEnabled = true;
  AutoRulePure r = rule(AUTO_TRIG_CHANGE, AUTO_SIG_GEAR, 0, BC_GEAR_P, BC_GEAR_R);
  r.act[0] = {AUTO_ACT_HAZARD_BLINK, 0, 4};
  r.act[1] = {AUTO_ACT_WINDOWS, 0x01, BC_WINREQ_VENT};
  r.dryRun = true;
  put(5, r);
  car.gear = BC_GEAR_R;
  run(1000);
  car.gear = BC_GEAR_P;
  run(1000);
  assert(autoRt[5].fires == 1 && logHas("dry-run (not sent)", AUTO_ACT_HAZARD_BLINK));
  assert(!bcState.blinkActive && windowCommands(BC_WINREQ_VENT).empty());
  // Real run: hazard blink with its own duration even though Hazard Blink is off.
  autoRules[5].dryRun = false;
  car.gear = BC_GEAR_R; run(1000);
  car.gear = BC_GEAR_P; run(300);
  assert(bcState.blinkActive && bcState.blinkFromAuto && bcState.blinkSeconds == 4 && !bcCfg.hzBlinkEnabled);
  run(1000);
  assert(!windowCommands(BC_WINREQ_VENT).empty());
  // Listen-only: CAN actions are skipped, not sent.
  g_passive = true;
  bcState.blinkActive = false;
  car.gear = BC_GEAR_R; run(1500);
  car.gear = BC_GEAR_P; run(1000);
  assert(logHas("listen-only (not sent)", AUTO_ACT_WINDOWS) && !bcState.blinkActive);
  g_passive = false;

  // Busy: a second window command waits for the first instead of failing.
  resetAll();
  autoEnabled = true;
  AutoRulePure a = rule(AUTO_TRIG_STATE, AUTO_SIG_FLAG_BASE, AUTO_OP_EQ, 1);
  a.act[0] = {AUTO_ACT_WINDOWS, 0x01, BC_WINREQ_VENT};
  AutoRulePure b = a;
  b.act[0] = {AUTO_ACT_WINDOWS, 0x02, BC_WINREQ_CLOSED};
  put(6, a);
  put(7, b);
  run(1000);
  autoFlags = 1;
  run(2500);
  assert(!windowCommands(BC_WINREQ_VENT).empty());
  // takeTx above consumed nothing from the list; read both from g_tx.
  bool vent = false, close = false;
  for (const auto &t : g_tx) {
    if (t.id != 0x294) continue;
    if (bcGetBitsPure(t.data, 52, 3) == BC_WINREQ_VENT) vent = true;
    if (bcGetBitsPure(t.data, 52, 3) == BC_WINREQ_CLOSED && bcGetBitsPure(t.data, bcWindowRequestedBitPure(1), 1)) close = true;
  }
  assert(vent && close && logHas("OK", AUTO_ACT_WINDOWS) && !logHas("busy (gave up)"));
}

static void testFeatureAndPedalActions() {
  resetAll();
  autoEnabled = true;
  AutoRulePure r = rule(AUTO_TRIG_STATE, AUTO_SIG_FLAG_BASE + 1, AUTO_OP_EQ, 1);
  r.act[0] = {AUTO_ACT_FEATURE, AUTO_FEAT_EASY_ENTRY, 1};
  r.act[1] = {AUTO_ACT_FEATURE, AUTO_FEAT_TLSSC, 1};
  r.act[2] = {AUTO_ACT_PEDAL_MODE, 2, 0};
  put(8, r);
  run(1000);
  autoFlags = 0x02;
  run(500);
  assert(bcCfg.eeEnabled && tlsscEnabled);
  assert(g_pedalRequests.size() == 1 && g_pedalRequests[0] == PEDAL_MAP_RAW_PERFORMANCE);
  run(2000);
  assert(g_nvs.count("bodycmf/cfg") == 1);  // Body Comfort switch saved
  // Again: already set, no extra toggles.
  autoFlags = 0; run(300); autoFlags = 0x02; run(1500);
  assert(autoRt[8].fires == 2 && tlsscEnabled && bcCfg.eeEnabled && logHas("already set", AUTO_ACT_FEATURE));
  // Toggle an existing feature off, and a feature state as a trigger.
  AutoRulePure t = rule(AUTO_TRIG_STATE, AUTO_SIG_FLAG_BASE + 2, AUTO_OP_EQ, 1);
  t.act[0] = {AUTO_ACT_FEATURE, AUTO_FEAT_TLSSC, 2};
  put(9, t);
  AutoRulePure w = rule(AUTO_TRIG_STATE, AUTO_SIG_FEATURE_BASE + AUTO_FEAT_TLSSC, AUTO_OP_EQ, 0);
  w.act[0] = {AUTO_ACT_FLAG, 7, 1};
  put(10, w);
  run(500);
  autoFlags |= 0x04;
  run(1000);
  assert(!tlsscEnabled && (autoFlags & 0x80u));
  // Listen-only as a feature (existing toggle path).
  AutoRulePure lo = rule(AUTO_TRIG_STATE, AUTO_SIG_FLAG_BASE + 4, AUTO_OP_EQ, 1);
  lo.act[0] = {AUTO_ACT_FEATURE, AUTO_FEAT_LISTEN_ONLY, 1};
  put(11, lo);
  run(300);
  autoFlags |= 0x10;
  run(500);
  assert(g_passive && g_listenRequests == 1);
  g_passive = false;
}

static void testRateLimit() {
  resetAll();
  autoEnabled = true;
  AutoRulePure r = rule(AUTO_TRIG_CHANGE, AUTO_SIG_DOOR_RR, 0, AUTO_ANY, AUTO_ANY);
  r.act[0] = {AUTO_ACT_FLAG, 5, 2};
  put(12, r);
  run(1000);
  for (int i = 0; i < 40; ++i) {
    car.latch[BC_DOOR_RR] = (i % 2) ? 2 : 1;
    run(1100);
  }
  assert(autoRt[12].fires == 40);
  assert(autoRateLimited >= 1 && logHas("rate limited", AUTO_ACT_FLAG));
}

static void testSignals() {
  resetAll();
  car.gear = BC_GEAR_R;
  car.kph10 = -35;
  car.pedalRaw = 125;  // 50 %
  car.brake = 1;
  car.lockSimple = 2;
  car.latch[BC_DOOR_RL] = 1;
  car.handle[1] = true;
  car.winPosState[0] = BC_WINPOS_CRACKED;
  car.winPosState[3] = BC_WINPOS_FULL;
  car.snowy = true;
  car.mirror[0] = BC_MIRROR_FOLDING;
  car.camera = true;
  car.hazard = 1;
  car.rearUnbuckled[1] = 1;
  tlsscEnabled = true;
  autoFlags = 0x40;
  run(1000);
  AutoSignalsPure s;
  bool gf = false; uint8_t gear = 0;
  autoSnapshotSignalsLocked(g_now, s, gf, gear);
  assert(gf && s.value[AUTO_SIG_GEAR] == BC_GEAR_R);
  assert(s.valid[AUTO_SIG_SPEED] && s.value[AUTO_SIG_SPEED] == -4);
  assert(s.value[AUTO_SIG_PEDAL] == 50 && s.value[AUTO_SIG_BRAKE] == 1);
  assert(s.value[AUTO_SIG_DOOR_RL] == 1 && s.value[AUTO_SIG_DOOR_FL] == 0 && s.value[AUTO_SIG_ANY_DOOR] == 1);
  assert(s.value[AUTO_SIG_HANDLE_FR] == 1 && s.value[AUTO_SIG_HANDLE_FL] == 0);
  assert(s.valid[AUTO_SIG_LOCKED] && s.value[AUTO_SIG_LOCKED] == 1);
  assert(s.value[AUTO_SIG_WIN_FL] == 1 && s.value[AUTO_SIG_WIN_RR] == 3 && s.value[AUTO_SIG_WIN_FR] == 0);
  assert(s.value[AUTO_SIG_ANY_WIN_OPEN] == 1);
  assert(s.value[AUTO_SIG_SNOW] == 1 && s.value[AUTO_SIG_RAIN] == 0 && s.value[AUTO_SIG_RAIN_OR_SNOW] == 1);
  assert(s.value[AUTO_SIG_MIRRORS] == 2 && s.value[AUTO_SIG_CAMERA] == 1 && s.value[AUTO_SIG_HAZARDS] == 1);
  assert(s.valid[AUTO_SIG_LIGHT_SWITCH] && s.value[AUTO_SIG_LIGHT_SWITCH] == 0);
  assert(s.value[AUTO_SIG_BELT_2C] == 1 && s.value[AUTO_SIG_BELT_2L] == 0);
  assert(s.value[AUTO_SIG_FEATURE_BASE + AUTO_FEAT_TLSSC] == 1);
  assert(s.value[AUTO_SIG_FEATURE_BASE + AUTO_FEAT_EASY_ENTRY] == 0);
  assert(s.value[AUTO_SIG_FLAG_BASE + 6] == 1 && s.value[AUTO_SIG_FLAG_BASE] == 0);
  // Stale: nothing valid except features / flags.
  g_now += 5000;
  autoSnapshotSignalsLocked(g_now, s, gf, gear);
  assert(!gf && !s.valid[AUTO_SIG_GEAR] && !s.valid[AUTO_SIG_DOOR_FL] && !s.valid[AUTO_SIG_ANY_DOOR]);
  assert(!s.valid[AUTO_SIG_WIN_FL] && !s.valid[AUTO_SIG_LOCKED] && s.valid[AUTO_SIG_FLAG_BASE]);
  tlsscEnabled = false;
}

static void testHttpAndPersistence() {
  resetAll();
  // Save a rule through the API.
  server.args = {{"idx", "3"}, {"name", "Rain \"close\""},
                 {"r", "1,0,0,30, 1,20,0,1,-32768,0, 1,0,0,1, 12,0,0,1, 0,0,0,0, 0,0,0,0, 3,15,3, 0,0,0, 0,0,0"}};
  httpAutomationRule();
  assert(server.lastCode == 400);  // spaces are not allowed
  server.args["r"] = "1,0,0,30,1,20,0,1,-32768,0,1,0,0,1,12,0,1,1,0,0,0,0,0,0,0,0,3,15,3,0,0,0,0,0,0";
  httpAutomationRule();
  assert(server.lastCode == 200);
  assert(autoRules[3].used && autoRules[3].trig.signal == AUTO_SIG_RAIN_OR_SNOW);
  assert((autoRules[3].cond[1].opFlags & AUTO_COND_OR) && autoRules[3].cooldownS == 30);
  assert(strcmp(autoRules[3].name, "Rain close") == 0);
  assert(server.lastBody.find("\"idx\":3,\"name\":\"Rain close\",\"r\":[1,0,0,30,1,20,0,1,-32768,0,1,0,0,1,12,0,1,1,") != std::string::npos);
  assert(g_nvs.count("bcauto/rules") == 1);
  // Invalid rules are refused with the reason.
  server.args = {{"idx", "4"}, {"r", "1,0,0,0,1,20,0,1,-32768,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,15,3,0,0,0,0,0,0"}};
  httpAutomationRule();
  assert(server.lastCode == 400 && server.lastBody.find("cooldown") != std::string::npos);
  server.args = {{"idx", "4"}, {"r", "1,0,0,5,1,20,0,1,-32768,0"}};
  httpAutomationRule();
  assert(server.lastCode == 400 && server.lastBody.find("35 fields") != std::string::npos);
  server.args = {{"idx", "16"}, {"r", "1"}};
  httpAutomationRule();
  assert(server.lastCode == 400);
  server.args = {{"idx", "4"}, {"r", "1,0,0,5,1,20,0,1,-32768,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0"}};
  httpAutomationRule();
  assert(server.lastCode == 200 && autoRules[4].act[0].type == AUTO_ACT_FRUNK);
  // Global switch and flags.
  server.args = {{"enabled", "1"}};
  httpAutomationGlobal();
  assert(server.lastCode == 200 && autoEnabled && server.lastBody.find("\"enabled\":true") != std::string::npos);
  server.args = {{"idx", "2"}, {"value", "1"}};
  httpAutomationFlag();
  assert(server.lastCode == 200 && autoFlags == 0x04);
  // Run now works with the master switch off, honours dry-run and the block list.
  server.args = {{"enabled", "0"}};
  httpAutomationGlobal();
  car.gear = BC_GEAR_P;
  run(1000);
  server.args = {{"idx", "3"}};
  httpAutomationRun();
  assert(server.lastCode == 200);
  run(1000);
  assert(!windowCommands(BC_WINREQ_CLOSED).empty() && logHas("run now", AUTO_LOG_MANUAL));
  server.args = {{"idx", "9"}};
  httpAutomationRun();
  assert(server.lastCode == 409);
  // Catalog.
  httpAutomationCatalog();
  assert(server.lastCode == 200);
  assert(server.lastBody.find("{\"id\":17,\"key\":\"anyWindowOpen\",\"kind\":0,\"min\":0,\"max\":1}") != std::string::npos);
  assert(server.lastBody.find("\"driveBlocked\":[[2,1],[6,1],[5,255]]") != std::string::npos);
  assert(server.lastBody.find("\"key\":\"flag8\"") != std::string::npos);
  // Reload from NVS.
  for (auto &x : autoRules) x = {};
  autoEnabled = true;
  autoFlags = 0;
  automationLoad();
  assert(autoRules[3].used && autoRules[4].used && !autoRules[0].used && !autoEnabled && autoFlags == 0x04);
  assert(strcmp(autoRules[3].name, "Rain close") == 0);
  // Delete.
  server.args = {{"idx", "3"}};
  httpAutomationDelete();
  assert(server.lastCode == 200 && !autoRules[3].used);
  automationLoad();
  assert(!autoRules[3].used && autoRules[4].used);
  // Status JSON.
  httpAutomationGet();
  assert(server.lastBody.find("\"rules\":[{\"idx\":4,") != std::string::npos);
  assert(server.lastBody.find("\"log\":[{") != std::string::npos);
}

int main() {
  testSignals();
  testRainClosesWindows();
  testDelayCooldownRestart();
  testDriveBlockList();
  testDryRunListenOnlyAndBusy();
  testFeatureAndPedalActions();
  testRateLimit();
  testHttpAndPersistence();
  std::puts("automation_runtime host: all tests passed");
  return 0;
}
