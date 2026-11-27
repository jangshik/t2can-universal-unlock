// Host harness for body_comfort_runtime.h: compiles the real runtime against
// minimal Arduino / FreeRTOS / CAN stubs and drives it with simulated Juniper
// Body CAN traffic. Build:
//   g++ -std=c++17 -Wall -Wextra -Werror -I. -Itests tests/test_body_comfort_runtime_host.cpp
#include "body_comfort_host_harness.h"

static void testUnsupportedTopologies() {
  resetAll();
  activeVehicleTopology = VEHICLE_TOPOLOGY_STANDARD_PARTY_CHASSIS;
  assert(bodyComfortActiveBus() == BC_BUS_NONE);
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.sbEnabled = true;
  setCfg(c);
  car.rearUnbuckled[0] = 1;
  run(500);
  assert(g_tx.empty());
  activeVehicleTopology = VEHICLE_TOPOLOGY_STANDARD_BODY_CHASSIS;
  activeVehicleProfile = VEHICLE_MODEL_S_2017;
  assert(bodyComfortActiveBus() == BC_BUS_NONE);
  activeVehicleProfile = VEHICLE_MODEL_YL;
  activeVehicleTopology = VEHICLE_TOPOLOGY_YL_PARTY_VH;
  assert(bodyComfortActiveBus() == BC_BUS_B);
  // Frames from the wrong bus are ignored.
  g_bus = BC_BUS_A;
  run(500);
  assert(g_tx.empty());
  g_bus = BC_BUS_B;
  run(300);
  assert(!takeTx(0x3A1).empty() && takeTx(0x3A1)[0].bus == BC_BUS_B);
  activeVehicleProfile = VEHICLE_MODEL_Y_JUNIPER;
  activeVehicleTopology = VEHICLE_TOPOLOGY_STANDARD_BODY_CHASSIS;
  g_bus = BC_BUS_A;
  vehicleProfileSetupMode = true;
  assert(bodyComfortActiveBus() == BC_BUS_NONE);
  vehicleProfileSetupMode = false;
}

static void testAllOffByDefault() {
  resetAll();
  car.gear = BC_GEAR_R;
  car.rearUnbuckled[0] = car.rearUnbuckled[1] = car.rearUnbuckled[2] = 1;
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(2000);
  assert(g_tx.empty());
}

static void testEasyEntryOpenAndRestore() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.eeEnabled = true;
  setCfg(c);
  // Teach travel: rear-left seen closed, fully open, then left at 50 %.
  run(300);
  car.winPosState[BC_DOOR_RL] = BC_WINPOS_FULL; car.winPos[BC_DOOR_RL] = 200;
  run(300);
  car.winPosState[BC_DOOR_RL] = BC_WINPOS_PARTIAL; car.winPos[BC_DOOR_RL] = 0;  // 50 % open
  run(2000);
  // Driver door opens in P: FL goes fully down.
  car.latch[BC_DOOR_FL] = 1;
  run(700);
  auto w = takeTx(0x294);
  assert(w.size() == BC_WINDOW_CMD_FRAMES);
  for (const auto &r : w) {
    assert(bcGetBitsPure(r.data, 52, 3) == BC_WINREQ_OPEN && bcGetBitsPure(r.data, 42, 6) == 20u);
    assert(bcGetBitsPure(r.data, 48, 4) == 0x1u);  // FL only
    assert(r.data[0] == 0x40);                      // stock bytes preserved
  }
  assert(bcState.entry[BC_DOOR_FL].phase == BC_ENTRY_OPENED);
  g_tx.clear();
  car.winPosState[BC_DOOR_FL] = BC_WINPOS_FULL; car.winPos[BC_DOOR_FL] = 200;
  run(1000);
  // Door closes: nothing before the restore delay, then GOTO_CLOSED.
  car.latch[BC_DOOR_FL] = 2;
  run(500);
  assert(takeTx(0x294).empty());
  run(1100);
  w = takeTx(0x294);
  assert(w.size() == BC_WINDOW_CMD_FRAMES);
  assert(bcGetBitsPure(w[0].data, 52, 3) == BC_WINREQ_CLOSED);
  assert(bcGetBitsPure(w[0].data, 48, 4) == 0x1u);
  assert(bcState.entry[BC_DOOR_FL].phase == BC_ENTRY_IDLE);
  assert(bcState.entryOpens == 1 && bcState.entryRestores == 1);

  // Rear-left partially open (50 %): restore uses GOTO_PERCENT 50.
  g_tx.clear();
  car.latch[BC_DOOR_RL] = 1;
  run(700);
  w = takeTx(0x294);
  assert(!w.empty() && bcGetBitsPure(w[0].data, 48, 4) == 0x4u);
  car.winPosState[BC_DOOR_RL] = BC_WINPOS_FULL; car.winPos[BC_DOOR_RL] = 200;
  run(500);
  g_tx.clear();
  car.latch[BC_DOOR_RL] = 2;
  run(1500);
  w = takeTx(0x294);
  assert(!w.empty());
  assert(bcGetBitsPure(w[0].data, 52, 3) == BC_WINREQ_PERCENT);
  assert(bcGetBitsPure(w[0].data, 42, 6) == 10u);  // 50 % / 5
}

static void testEasyEntryBlocked() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.eeEnabled = true;
  setCfg(c);
  car.rainy = true;
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(500);
  assert(takeTx(0x294).empty() && bcState.entryBlockedWeather == 1);
  // Not P.
  car.latch[BC_DOOR_FL] = 2;
  car.rainy = false;
  car.gear = BC_GEAR_D;
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(500);
  assert(takeTx(0x294).empty() && bcState.entryBlockedGear == 1);
  // Door excluded by mask.
  car.latch[BC_DOOR_FL] = 2;
  car.gear = BC_GEAR_P;
  c.eeDoorMask = 0x0Eu;
  setCfg(c);
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(500);
  assert(takeTx(0x294).empty());
  // Snowy also blocks; disabling weather block allows it.
  car.latch[BC_DOOR_FL] = 2;
  c.eeDoorMask = 0x0Fu;
  car.snowy = true;
  setCfg(c);
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(500);
  assert(takeTx(0x294).empty() && bcState.entryBlockedWeather == 2);
  car.latch[BC_DOOR_FL] = 2;
  c.eeWeatherBlock = false;
  setCfg(c);
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(500);
  assert(!takeTx(0x294).empty());
}

static void testMirrorFoldCameraUnfold() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.eeEnabled = true;
  c.eeMirrorFold = true;
  setCfg(c);
  run(2000);
  // Passenger door: the driver door never closes, so the speed fallback unfolds.
  car.latch[BC_DOOR_FR] = 1;
  run(400);
  auto m = takeTx(0x273);
  assert(m.size() == BC_ONE_SHOT_FRAMES);
  assert(bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_RETRACT && m[0].data[2] == 0x11);
  assert(bcState.mirrorFoldedByUs && bcState.mirrorFoldedByEntry && bcState.mirrorFolds == 1);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  car.latch[BC_DOOR_FR] = 2;
  run(2000);
  g_tx.clear();
  // D: camera button pressed once while folded.
  car.gear = BC_GEAR_D;
  run(500);
  auto cam = takeTx(0x3C2);
  assert(!cam.empty());
  assert(bcSwitchMuxPure(cam[0].data) == 1u && bcCameraButtonPressedPure(cam[0].data));
  car.camera = true;
  run(1000);
  assert(bcState.cameraOpenedByUs);
  // Speed above 10 kph for 3 s unfolds; exactly 10 kph never does.
  g_tx.clear();
  car.kph10 = 100;
  run(5000);
  assert(takeTx(0x273).empty());
  car.kph10 = 150;
  run(2500);
  assert(takeTx(0x273).empty());
  run(1000);
  m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_PRESENT);
  assert(!bcState.mirrorFoldedByUs);
  // The camera view we opened is closed again.
  run(500);
  cam = takeTx(0x3C2);
  assert(!cam.empty() && bcCameraButtonPressedPure(cam.back().data));
  assert(!bcState.cameraOpenedByUs);

  // Mirrors folded by the driver are never unfolded by this feature.
  resetAll();
  setCfg(c);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  car.gear = BC_GEAR_D;
  car.kph10 = 300;
  run(6000);
  assert(takeTx(0x273).empty());
}

static void testMirrorDriverDoorUnfold() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.eeEnabled = true;
  c.eeMirrorFold = true;
  setCfg(c);
  run(2000);
  // Easy Entry fold, driver door closes: unfold 2 s later, still in P.
  car.latch[BC_DOOR_FL] = 1;
  run(400);
  auto m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_RETRACT);
  assert(bcState.mirrorFoldedByEntry);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  g_tx.clear();
  run(3000);
  assert(takeTx(0x273).empty());  // door still open: stays folded
  car.latch[BC_DOOR_FL] = 2;
  run(1800);
  assert(takeTx(0x273).empty() && bcState.mirrorUnfoldPending);
  run(800);
  m = takeTx(0x273);
  assert(m.size() == BC_ONE_SHOT_FRAMES && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_PRESENT);
  assert(!bcState.mirrorFoldedByUs && !bcState.mirrorFoldedByEntry && bcState.mirrorDoorUnfolds == 1);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_UNFOLDED;
  g_tx.clear();
  run(3000);
  assert(takeTx(0x273).empty());

  // Driver door reopened within 2 s cancels; the next close re-arms it.
  resetAll();
  setCfg(c);
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(400);
  m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_RETRACT);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  g_tx.clear();
  car.latch[BC_DOOR_FL] = 2;
  run(1000);
  car.latch[BC_DOOR_FL] = 1;
  run(3000);
  assert(takeTx(0x273).empty() && !bcState.mirrorUnfoldPending && bcState.mirrorFoldedByUs);
  g_tx.clear();
  car.latch[BC_DOOR_FL] = 2;
  run(2400);
  m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_PRESENT);

  // A fold from the macro / dashboard action is not unfolded by the driver door.
  resetAll();
  c.eeEnabled = false;
  setCfg(c);
  run(2000);
  assert(bcRunActionLocked(g_now, BC_ACTION_MIRROR_FOLD_NOW) == nullptr);
  run(400);
  m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_RETRACT);
  assert(bcState.mirrorFoldedByUs && !bcState.mirrorFoldedByEntry);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  g_tx.clear();
  car.latch[BC_DOOR_FL] = 1;
  run(1000);
  car.latch[BC_DOOR_FL] = 2;
  run(4000);
  assert(takeTx(0x273).empty() && bcState.mirrorFoldedByUs && bcState.mirrorDoorUnfolds == 0);
}

static void testMirrorCooldown() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.eeEnabled = true;
  c.eeMirrorFold = true;
  c.eeMirrorCooldownS = 5;
  setCfg(c);
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(400);
  assert(!takeTx(0x273).empty());
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  car.latch[BC_DOOR_FL] = 2;
  run(2400);  // unfolded 2 s after the close: cooldown starts
  auto m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m.back().data, 24, 2) == BC_MIRROR_REQ_PRESENT);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_UNFOLDED;
  g_tx.clear();
  // Another door within the cooldown: the window still drops, the mirrors stay out.
  const uint32_t opensBefore = bcState.entryOpens;
  car.latch[BC_DOOR_FR] = 1;
  run(600);
  assert(bcState.entryOpens == opensBefore + 1u);
  assert(takeTx(0x273).empty() && bcState.mirrorFoldSkippedCooldown == 1u);
  assert(!bcState.mirrorFoldedByUs);
  server.args = {};
  httpBodyComfortConfig();
  assert(server.lastBody.find("\"mirrorCooldownLeftS\":") != std::string::npos);
  assert(server.lastBody.find("\"mirrorCooldownLeftS\":0,") == std::string::npos);
  car.latch[BC_DOOR_FR] = 2;
  run(4000);  // cooldown over
  car.latch[BC_DOOR_RL] = 1;
  run(400);
  m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_RETRACT);
  assert(bcState.mirrorFoldedByEntry);

  // Cooldown 0: folds again right away.
  resetAll();
  c.eeMirrorCooldownS = 0;
  setCfg(c);
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(400);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  car.latch[BC_DOOR_FL] = 2;
  run(2400);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_UNFOLDED;
  g_tx.clear();
  car.latch[BC_DOOR_FR] = 1;
  run(400);
  m = takeTx(0x273);
  assert(!m.empty() && bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_RETRACT);
  assert(bcState.mirrorFoldSkippedCooldown == 0u);
  server.args = {{"eeMirrorCooldownS", "601"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400 && bcCfg.eeMirrorCooldownS == 0u);
  server.args = {{"eeMirrorCooldownS", "120"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 200 && bcCfg.eeMirrorCooldownS == 120u);
  assert(server.lastBody.find("\"eeMirrorCooldownS\":120") != std::string::npos);
}

static void testSeatbelt() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.sbEnabled = true;
  c.sbSeatMask = 0x01u;  // 2nd-row left only
  setCfg(c);
  car.rearUnbuckled[0] = 1;
  car.rearUnbuckled[2] = 1;
  run(300);
  auto s = takeTx(0x3A1);
  assert(s.size() == 3);
  assert(bcGetBitsPure(s[0].data, 36, 2) == 0u);
  assert(bcGetBitsPure(s[0].data, 40, 2) == 1u);
  assert(s[0].data[7] == teslaAdditiveChecksumPure(0x3A1, s[0].data));
  g_tx.clear();
  car.rearUnbuckled[0] = 0;
  run(300);
  assert(takeTx(0x3A1).empty());  // nothing to suppress
}

static void testReverseHazardAndLight() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.rvHazard = true;
  c.rvLight = true;
  c.rvLightMode = BC_REAR_LIGHT_BOTH;
  setCfg(c);
  run(500);
  assert(g_tx.empty());
  car.gear = BC_GEAR_R;
  run(400);
  auto h = takeTx(0x3C2);
  assert(!h.empty() && bcSwitchMuxPure(h[0].data) == 0u && bcHazardButtonPressedPure(h[0].data));
  auto fog = takeTx(0x273);
  assert(!fog.empty() && bcGetBitsPure(fog[0].data, 23, 1) == 1u);
  auto lights = takeTx(0x3B3);
  assert(!lights.empty() && bcGetBitsPure(lights[0].data, 9, 3) == BC_LIGHT_SWITCH_ON);
  car.hazard = BC_HAZARD_REQ_BUTTON;
  run(1500);
  assert(bcState.hazardCtl.ownedOn);
  g_tx.clear();
  run(1000);
  assert(takeTx(0x3C2).empty());  // no repeated presses while on
  // Leave R: hazards off (we own them), overrides stop.
  car.gear = BC_GEAR_P;
  run(400);
  h = takeTx(0x3C2);
  assert(!h.empty() && bcHazardButtonPressedPure(h[0].data));
  car.hazard = 0;
  run(1500);
  assert(!bcState.hazardCtl.ownedOn);
  g_tx.clear();
  run(1000);
  assert(g_tx.empty());

  // R -> P with an unlock flash in between: hazards still turned off.
  car.gear = BC_GEAR_R;
  run(400);
  assert(!takeTx(0x3C2).empty());
  car.hazard = BC_HAZARD_REQ_BUTTON;
  run(1500);
  assert(bcState.hazardCtl.ownedOn);
  g_tx.clear();
  car.gear = BC_GEAR_P;
  car.hazard = 3;  // UNLOCK flash
  run(800);
  assert(takeTx(0x3C2).empty() && bcState.hazardCtl.ownedOn);
  car.hazard = BC_HAZARD_REQ_BUTTON;
  run(400);
  auto off = takeTx(0x3C2);
  assert(!off.empty() && bcHazardButtonPressedPure(off[0].data));
  car.hazard = 0;
  run(1500);
  assert(!bcState.hazardCtl.ownedOn);
  g_tx.clear();

  // Fog-only mode never touches 0x3B3; hazards on by the driver are left alone.
  resetAll();
  c.rvLightMode = BC_REAR_LIGHT_FOG;
  setCfg(c);
  car.hazard = BC_HAZARD_REQ_BUTTON;
  car.gear = BC_GEAR_R;
  run(1500);
  assert(takeTx(0x3B3).empty() && !takeTx(0x273).empty());
  assert(takeTx(0x3C2).empty());
  car.gear = BC_GEAR_P;
  g_tx.clear();
  run(1500);
  assert(takeTx(0x3C2).empty());
}

static void testDoorHazard() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.drHazard = true;
  setCfg(c);
  car.gear = BC_GEAR_D;
  run(500);
  assert(g_tx.empty());
  car.latch[BC_DOOR_RR] = 1;
  run(300);
  assert(!takeTx(0x3C2).empty());
  car.hazard = BC_HAZARD_REQ_BUTTON;
  run(1500);
  g_tx.clear();
  car.latch[BC_DOOR_RR] = 2;
  run(1800);
  assert(takeTx(0x3C2).empty());  // still inside the 2 s delay
  run(600);
  assert(!takeTx(0x3C2).empty());
}

// The hazard press ends as soon as VCFRONT_lighting shows the change, so the
// car sees one press (stock un-pressed frames no longer interleave with it).
static int hazardPressFrames() {
  int n = 0;
  for (const auto &t : g_tx)
    if (t.id == 0x3C2 && bcSwitchMuxPure(t.data) == 0u && bcHazardButtonPressedPure(t.data)) n++;
  return n;
}

static void testHazardPressReleasedOnConfirm() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.hzBlinkEnabled = true;
  c.hzBlinkSeconds = 3;
  setCfg(c);
  run(500);
  assert(bcStartHazardBlinkLocked(g_now) == nullptr);
  // The simulated car turns its hazards on as soon as it sees the first press.
  for (int i = 0; i < 100 && hazardPressFrames() == 0; ++i) run(10);
  assert(hazardPressFrames() == 1);
  car.hazard = BC_HAZARD_REQ_BUTTON;
  car.indicator = true;
  run(400);
  assert(hazardPressFrames() == 1 && bcState.hazardPressEarlyRelease == 1);
  assert(bcState.hazTraceConfirmMs >= 0 && bcState.hazTraceConfirmMs <= 200);
  car.indicator = false; run(400);
  car.indicator = true; run(400);
  assert(bcState.hazTraceCount == 3);
  server.args = {};
  httpBodyComfortConfig();
  assert(server.lastBody.find("\"hazardTrace\":\"press 1 fr · req +") != std::string::npos);
  assert(server.lastBody.find(" · lamp 1@") != std::string::npos);
  // Turning off: same, one frame.
  run(2600);
  assert(!bcState.blinkActive);
  g_tx.clear();
  for (int i = 0; i < 200 && hazardPressFrames() == 0; ++i) run(10);
  assert(hazardPressFrames() == 1);
  car.hazard = 0;
  car.indicator = false;
  run(400);
  assert(hazardPressFrames() == 1 && bcState.hazardPressEarlyRelease == 2);
  // A car that does not react keeps the full 300 ms press (unchanged behaviour).
  resetAll();
  setCfg(c);
  run(500);
  assert(bcStartHazardBlinkLocked(g_now) == nullptr);
  run(600);
  assert(hazardPressFrames() >= 3 && bcState.hazardPressEarlyRelease == 0);
}

static void testHazardBlinkAndMacros() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.hzBlinkEnabled = true;
  c.hzBlinkSeconds = 3;
  c.mcEnabled = true;
  c.mcAction[BC_BTN_RIGHT_PRESS][BC_GESTURE_LONG] = BC_ACTION_RUN_HAZARD_BLINK;
  c.mcAction[BC_BTN_LEFT_PRESS][BC_GESTURE_DOUBLE] = BC_ACTION_TOGGLE_REVERSE_HAZARD;
  setCfg(c);
  run(500);
  // Long press on the right scroll wheel.
  car.buttons[BC_BTN_RIGHT_PRESS] = true;
  run(1000);
  car.buttons[BC_BTN_RIGHT_PRESS] = false;
  run(100);
  assert(bcState.blinkActive && bcState.lastMacroAction == BC_ACTION_RUN_HAZARD_BLINK);
  run(300);
  assert(!takeTx(0x3C2).empty());
  // Hazards come on; they stay requested for 3 s from the moment they are seen on.
  car.hazard = BC_HAZARD_REQ_BUTTON;
  run(2800);
  assert(bcState.blinkActive);
  run(400);
  assert(!bcState.blinkActive);
  g_tx.clear();
  run(1500);
  assert(!takeTx(0x3C2).empty());  // turned back off
  car.hazard = 0;
  run(1500);

  // Double click on the left scroll wheel toggles Reverse Hazards and saves it.
  assert(!bcCfg.rvHazard);
  car.buttons[BC_BTN_LEFT_PRESS] = true; run(100);
  car.buttons[BC_BTN_LEFT_PRESS] = false; run(100);
  car.buttons[BC_BTN_LEFT_PRESS] = true; run(100);
  car.buttons[BC_BTN_LEFT_PRESS] = false; run(700);
  assert(bcCfg.rvHazard);
  run(2000);
  BodyComfortConfigPure saved = {};
  const auto &blob = g_nvs["bodycmf/cfg"];
  assert(bodyComfortDeserializePure(blob.data(), (uint32_t)blob.size(), saved) && saved.rvHazard);
  // Macros disabled: no action.
  bcCfg.mcEnabled = false;
  car.buttons[BC_BTN_LEFT_PRESS] = true; run(100);
  car.buttons[BC_BTN_LEFT_PRESS] = false; run(100);
  car.buttons[BC_BTN_LEFT_PRESS] = true; run(100);
  car.buttons[BC_BTN_LEFT_PRESS] = false; run(700);
  assert(bcCfg.rvHazard);
  // Voice and camera buttons: double voice toggles Easy Entry, long camera folds mirrors.
  bcCfg.mcEnabled = true;
  bcCfg.mcAction[BC_BTN_VOICE][BC_GESTURE_DOUBLE] = BC_ACTION_TOGGLE_EASY_ENTRY;
  bcCfg.mcAction[BC_BTN_CAMERA][BC_GESTURE_LONG] = BC_ACTION_MIRROR_FOLD_NOW;
  assert(!bcCfg.eeEnabled);
  car.buttons[BC_BTN_VOICE] = true; run(100);
  car.buttons[BC_BTN_VOICE] = false; run(100);
  car.buttons[BC_BTN_VOICE] = true; run(100);
  car.buttons[BC_BTN_VOICE] = false; run(700);
  assert(bcCfg.eeEnabled && bcState.lastMacroButton == BC_BTN_VOICE);
  g_tx.clear();
  car.buttons[BC_BTN_CAMERA] = true; run(1000);
  car.buttons[BC_BTN_CAMERA] = false; run(500);
  assert(bcState.lastMacroButton == BC_BTN_CAMERA && bcState.lastMacroGesture == BC_GESTURE_LONG);
  auto mf = takeTx(0x273);
  assert(!mf.empty() && bcGetBitsPure(mf[0].data, 24, 2) == BC_MIRROR_REQ_RETRACT);
  // Blink is rejected while disabled.
  bcCfg.hzBlinkEnabled = false;
  assert(bcRunActionLocked(g_now, BC_ACTION_RUN_HAZARD_BLINK) != nullptr);
}

static void testHttp() {
  resetAll();
  server.args = {{"eeEnabled", "1"}, {"eeRestoreDelayMs", "1500"}, {"mc", "1,2,8"}, {"rvLightMode", "0"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 200);
  assert(bcCfg.eeEnabled && bcCfg.eeRestoreDelayMs == 1500);
  assert(bcCfg.mcAction[1][2] == BC_ACTION_RUN_HAZARD_BLINK && bcCfg.rvLightMode == 0);
  assert(server.lastBody.find("\"eeEnabled\":true") != std::string::npos);
  assert(server.lastBody.find("\"mcAction\":[[0,0,0],[0,0,8],") != std::string::npos);
  assert(server.lastBody.find(",[0,0,0],[0,0,0]],\"hzBlinkEnabled\"") != std::string::npos);  // 8 buttons
  server.args = {{"mc", "7,0,10"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 200 && bcCfg.mcAction[BC_BTN_CAMERA][BC_GESTURE_DOUBLE] == BC_ACTION_CAMERA);
  assert(g_nvs.count("bodycmf/cfg") == 1);
  // Invalid input changes nothing.
  server.args = {{"eeRestoreDelayMs", "9000"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400 && bcCfg.eeRestoreDelayMs == 1500);
  server.args = {{"mc", "9,0,1"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400);
  server.args = {{"mc", "1,1"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400);
  server.args = {{"sbEnabled", "yes"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400 && !bcCfg.sbEnabled);
  server.args = {};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400);
  // Load round trip.
  bcCfg = bodyComfortDefaultsPure();
  bodyComfortCfgLoad();
  assert(bcCfg.eeEnabled && bcCfg.eeRestoreDelayMs == 1500);
  // Action endpoint.
  server.args = {{"id", "8"}};
  httpBodyComfortAction();
  assert(server.lastCode == 409);  // blink disabled
  server.args = {{"id", "99"}};
  httpBodyComfortAction();
  assert(server.lastCode == 400);
  server.args = {{"id", "9"}};
  httpBodyComfortAction();
  assert(server.lastCode == 200 && bcState.mirrorCmd.active);
  // Status JSON is well formed enough to contain every section.
  httpBodyComfortConfig();
  assert(server.lastBody.find("\"live\":{") != std::string::npos);
  assert(server.lastBody.find("\"stats\":{") != std::string::npos);
  assert(server.lastBody.find("\"bus\":\"BODY_CAN_A\"") != std::string::npos);
  activeVehicleTopology = VEHICLE_TOPOLOGY_STANDARD_PARTY_CHASSIS;
  server.args = {{"eeEnabled", "0"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 409);
  activeVehicleTopology = VEHICLE_TOPOLOGY_STANDARD_BODY_CHASSIS;
}

static void testExistingFeatureMacros() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.mcEnabled = true;
  c.mcAction[BC_BTN_VOICE][BC_GESTURE_DOUBLE] = BC_ACTION_TOGGLE_NAG_KILLER;
  c.mcAction[BC_BTN_CAMERA][BC_GESTURE_LONG] = BC_ACTION_TOGGLE_TLSSC;
  c.mcAction[BC_BTN_CAMERA][BC_GESTURE_TRIPLE] = BC_ACTION_TOGGLE_CONTINUOUS_AP;
  setCfg(c);
  run(500);
  auto dbl = [](uint8_t b) {
    car.buttons[b] = true; run(100); car.buttons[b] = false; run(100);
    car.buttons[b] = true; run(100); car.buttons[b] = false; run(700);
  };
  auto tpl = [](uint8_t b) {
    for (int i = 0; i < 3; ++i) { car.buttons[b] = true; run(100); car.buttons[b] = false; run(100); }
    run(600);
  };
  // Nag Killer: same commit path as the dashboard, admin hold released.
  dbl(BC_BTN_VOICE);
  assert(nagCfg.enabled && g_nagCommits == 1 && g_adminHoldDepth == 0);
  assert(std::string(bcState.lastMacroResult) == "OK");
  dbl(BC_BTN_VOICE);
  assert(!nagCfg.enabled && g_nagCommits == 2);
  // TLSSC long press.
  car.buttons[BC_BTN_CAMERA] = true; run(1000); car.buttons[BC_BTN_CAMERA] = false; run(200);
  assert(tlsscEnabled);
  // Continuous AP without a chosen method is refused with a reason.
  tpl(BC_BTN_CAMERA);
  assert(!g_contAp.enabled && std::string(bcState.lastMacroResult).find("method") != std::string::npos);
  g_contAp.method = ContApMethod::ScrollDouble;
  tpl(BC_BTN_CAMERA);
  assert(g_contAp.enabled && g_contAp.method == ContApMethod::ScrollDouble);
  // Failures leave the feature unchanged and report why; holds stay balanced.
  g_quiesceOk = false;
  dbl(BC_BTN_VOICE);
  assert(!nagCfg.enabled && g_adminHoldDepth == 0);
  assert(std::string(bcState.lastMacroResult).find("TSL9") != std::string::npos);
  g_quiesceOk = true;
  g_saveOk = false;
  dbl(BC_BTN_VOICE);
  assert(!nagCfg.enabled && g_adminHoldDepth == 0 && std::string(bcState.lastMacroResult) == "save failed");
  g_saveOk = true;
  // Every existing feature is reachable through the action endpoint too.
  server.args = {{"id", "12"}}; httpBodyComfortAction(); assert(server.lastCode == 200 && g_isa);
  server.args = {{"id", "13"}}; httpBodyComfortAction(); assert(server.lastCode == 200 && g_dms);
  server.args = {{"id", "14"}}; httpBodyComfortAction(); assert(server.lastCode == 200 && blinkAEnabled);
  server.args = {{"id", "16"}}; httpBodyComfortAction(); assert(server.lastCode == 200 && g_doorCancel);
  server.args = {{"id", "17"}}; httpBodyComfortAction(); assert(server.lastCode == 200 && g_vision);
  server.args = {{"id", "19"}}; httpBodyComfortAction(); assert(server.lastCode == 200 && preheatEnabled);
  g_supported = false;
  server.args = {{"id", "12"}}; httpBodyComfortAction(); assert(server.lastCode == 200 && !g_isa);  // turning OFF always allowed
  server.args = {{"id", "12"}}; httpBodyComfortAction(); assert(server.lastCode == 409 && !g_isa);
  g_supported = true;
  // Retired action 4 is rejected everywhere.
  server.args = {{"id", "4"}}; httpBodyComfortAction(); assert(server.lastCode == 400);
  server.args = {{"mc", "0,0,4"}}; httpBodyComfortUpdate(); assert(server.lastCode == 400);
  server.args = {{"mc", "0,0,19"}}; httpBodyComfortUpdate(); assert(server.lastCode == 200);
  server.args = {{"mc", "0,0,22"}}; httpBodyComfortUpdate(); assert(server.lastCode == 200);
  server.args = {{"mc", "0,0,23"}}; httpBodyComfortUpdate(); assert(server.lastCode == 400);
}

static void testKickdown() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.kdEnabled = true;
  c.kdTriggerPct = 80;
  c.kdReleasePct = 20;
  c.kdTargetRaw = BC_KD_TARGET_SPORT;
  setCfg(c);
  car.gear = BC_GEAR_D;
  car.pedalRaw = 150;  // 60 %
  run(1000);
  assert(g_pedalRequests.empty());
  car.pedalRaw = 210;  // 84 %
  run(300);
  assert(g_pedalRequests.size() == 1 && g_pedalRequests[0] == PEDAL_MAP_RAW_PERFORMANCE);
  assert(bcState.kickdown.kicked && bcState.kickdownEngages == 1);
  // Staying above, or dipping to 50 %, keeps the kicked mode.
  car.pedalRaw = 125;
  run(2000);
  assert(g_pedalRequests.size() == 1);
  // Pedal at/below 20 % for 0.5 s returns to Comfort (Chill).
  car.pedalRaw = 40;   // 16 %
  run(300);
  assert(g_pedalRequests.size() == 1);
  run(500);
  assert(g_pedalRequests.size() == 2 && g_pedalRequests[1] == PEDAL_MAP_RAW_CHILL);
  assert(!bcState.kickdown.kicked);
  // Brake releases immediately; brake held never engages.
  car.pedalRaw = 230;
  run(300);
  assert(g_pedalRequests.size() == 3 && bcState.kickdown.kicked);
  car.brake = 1;
  run(150);
  assert(g_pedalRequests.size() == 4 && g_pedalRequests[3] == PEDAL_MAP_RAW_CHILL);
  run(1000);
  assert(g_pedalRequests.size() == 4);
  car.brake = 0;
  // Brake release disabled: brake no longer releases; release mode "stock".
  bcCfg.kdReleaseOnBrake = false;
  bcCfg.kdReleaseMode = BC_KD_RELEASE_STOCK;
  run(300);
  assert(g_pedalRequests.size() == 5 && bcState.kickdown.kicked);
  car.brake = 1;
  car.pedalRaw = 0;
  run(200);
  assert(g_pedalRequests.size() == 5);
  run(500);
  assert(g_pedalRequests.size() == 6 && g_pedalRequests[5] == PEDAL_MAP_RAW_STOCK);
  car.brake = 0;
  // Refused request: retried after 300 ms, state stays consistent.
  bcCfg.kdReleaseOnBrake = true;
  bcCfg.kdReleaseMode = BC_KD_RELEASE_COMFORT;
  g_pedalMapOk = false;
  car.pedalRaw = 240;
  run(250);
  assert(!bcState.kickdown.kicked && bcState.kickdownRefused >= 1);
  g_pedalMapOk = true;
  run(500);
  assert(bcState.kickdown.kicked);
  // Leaving D releases; a refused release outside D is not retried.
  g_pedalMapOk = false;
  car.gear = BC_GEAR_P;
  car.pedalRaw = 0;
  const size_t before = g_pedalRequests.size();
  run(1000);
  assert(!bcState.kickdown.kicked && g_pedalRequests.size() == before + 1);
  g_pedalMapOk = true;
  // Not in D: no engage. Disabled: no engage.
  car.pedalRaw = 250;
  run(1000);
  assert(g_pedalRequests.size() == before + 1);
  car.gear = BC_GEAR_D;
  bcCfg.kdEnabled = false;
  run(1000);
  assert(g_pedalRequests.size() == before + 1);
  // Unsupported pedal-map profile: never engages.
  bcCfg.kdEnabled = true;
  g_supported = false;
  run(1000);
  assert(g_pedalRequests.size() == before + 1);
  g_supported = true;
  // HTTP validation.
  server.args = {{"kdTriggerPct", "30"}, {"kdReleasePct", "28"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400);
  server.args = {{"kdReleaseOnPedal", "0"}, {"kdReleaseOnBrake", "0"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400);
  server.args = {{"kdTriggerPct", "70"}, {"kdReleasePct", "10"}, {"kdTargetRaw", "1"}, {"kdReleaseMode", "1"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 200 && bcCfg.kdTriggerPct == 70 && bcCfg.kdReleaseMode == 1);
  assert(server.lastBody.find("\"kickdown\":") != std::string::npos);
  server.args = {{"kdTargetRaw", "3"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400);
}

static void testDoorHandleFrunk() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.dhEnabled = true;
  c.dhDoorMask = 0x01u;  // driver only
  c.dhHoldS = 2;
  setCfg(c);
  run(500);
  // Pull, door opens (latch OPENED), keep holding for `ms`, then release and close.
  auto pullOpen = [](uint8_t side, uint32_t ms) {
    const uint8_t door = side == 0 ? BC_DOOR_FL : BC_DOOR_FR;
    car.handle[side] = true; car.latch[door] = 1; run(ms);
    car.handle[side] = false; run(300); car.latch[door] = 2; run(300);
  };
  // Short pull (normal door opening): nothing.
  pullOpen(0, 800);
  assert(takeTx(0x273).empty());
  // Passenger not selected.
  pullOpen(1, 3000);
  assert(takeTx(0x273).empty());
  // Unlocked + open + held >= 2 s: frunk opens once.
  car.handle[0] = true; car.latch[BC_DOOR_FL] = 1; run(1800);
  assert(takeTx(0x273).empty());
  run(600);
  auto f = takeTx(0x273);
  assert(!f.empty() && bcGetBitsPure(f[0].data, 5, 1) == 1u && f[0].data[2] == 0x11);
  assert(bcState.lastMacroButton == BC_HANDLE_BUTTON_BASE && bcState.lastMacroAction == BC_ACTION_OPEN_FRUNK);
  g_tx.clear();
  run(3000);  // still held: no repeat
  assert(takeTx(0x273).empty());
  car.handle[0] = false; run(300); car.latch[BC_DOOR_FL] = 2; run(300);
  assert(bcState.frunkOpens == 1);
  // Door closed (handle pulled but latch stays CLOSED): never fires.
  car.handle[0] = true; run(4000); car.handle[0] = false; run(300);
  assert(takeTx(0x273).empty() && bcState.handleBlockedClosed == 1);
  // Car locked: never fires, even with the latch reported open.
  car.lockSimple = 2; car.frontLocked[0] = true;
  pullOpen(0, 4000);
  assert(takeTx(0x273).empty() && bcState.handleBlockedLocked == 1);
  // Overall unlocked but the driver door still locked (selective unlock): blocked.
  car.lockSimple = 1;
  pullOpen(0, 4000);
  assert(takeTx(0x273).empty());
  car.frontLocked[0] = false;
  // No lock data (0x339 absent): fail closed.
  car.lockSimple = 0;
  run(3500);
  pullOpen(0, 4000);
  assert(takeTx(0x273).empty());
  // Pull while locked: nothing; car unlocks and door opens mid-pull, hold time
  // starts then and the frunk opens 2 s later.
  car.lockSimple = 2; car.frontLocked[0] = true;
  run(500);
  car.handle[0] = true; run(1500);
  car.lockSimple = 1; car.frontLocked[0] = false; car.latch[BC_DOOR_FL] = 1;
  run(1800);
  assert(takeTx(0x273).empty());
  run(500);
  assert(!takeTx(0x273).empty());
  car.handle[0] = false; run(300); car.latch[BC_DOOR_FL] = 2; run(300);
  // Both doors selected: passenger works too.
  bcCfg.dhDoorMask = 0x03u;
  pullOpen(1, 2500);
  assert(!takeTx(0x273).empty());
  // Not in P: refused, nothing sent.
  g_tx.clear();
  car.gear = BC_GEAR_D;
  pullOpen(0, 2500);
  assert(takeTx(0x273).empty() && std::string(bcState.lastMacroResult) == "frunk only opens in P");
  car.gear = BC_GEAR_P;
  // Door-handle action can be any macro action.
  bcCfg.dhAction = BC_ACTION_TOGGLE_REVERSE_HAZARD;
  pullOpen(0, 2500);
  assert(bcCfg.rvHazard);
  server.args = {{"dhAction", "4"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400);
  server.args = {{"id", "20"}};
  httpBodyComfortAction();
  assert(server.lastCode == 200);
  httpBodyComfortConfig();
  assert(server.lastBody.find("\"lock\":\"UNLOCKED\"") != std::string::npos);
}

static void testListenOnly() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.mcEnabled = true;
  c.sbEnabled = true;
  c.mcAction[BC_BTN_VOICE][BC_GESTURE_LONG] = BC_ACTION_TOGGLE_LISTEN_ONLY;
  setCfg(c);
  car.rearUnbuckled[0] = 1;
  run(300);
  assert(!takeTx(0x3A1).empty());
  // Long voice press: listen-only ON through the same transition the USB logger uses.
  car.buttons[BC_BTN_VOICE] = true; run(1000); car.buttons[BC_BTN_VOICE] = false; run(200);
  assert(g_passive && g_listenRequests == 1);
  g_tx.clear();
  const uint32_t failsBefore = bcState.txFail;
  run(1000);
  assert(g_tx.empty() && bcState.txFail == failsBefore);  // nothing sent, not counted as failure
  // Macros still work in listen-only: long press again turns it off.
  car.buttons[BC_BTN_VOICE] = true; run(1000); car.buttons[BC_BTN_VOICE] = false; run(200);
  assert(!g_passive && g_listenRequests == 2);
  run(300);
  assert(!takeTx(0x3A1).empty());
  httpBodyComfortConfig();
  assert(server.lastBody.find("\"listenOnly\":false") != std::string::npos);
}

static void testWindowDelivery() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.eeEnabled = true;
  setCfg(c);
  // UI sends 0x294 once (with a horn tap in it), then stays silent.
  car.vc3Mode = 1; car.vc3Once = true;
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(1500);
  auto w = takeTx(0x294);
  assert(w.size() == BC_WINDOW_CMD_FRAMES);
  for (const auto &f : w) {
    assert(bcGetBitsPure(f.data, 52, 3) == BC_WINREQ_OPEN && bcGetBitsPure(f.data, 48, 4) == 1u);
    assert(f.data[2] == 0xC0);                    // persistent stock fields kept
    assert(bcGetBitsPure(f.data, 17, 1) == 0u);   // old horn request not replayed
    assert(bcGetBitsPure(f.data, 6, 2) == 0u);    // old key-pair response not replayed
  }
  assert(bcState.lastWinOutcome == BC_WIN_OUTCOME_SENT && bcState.lastWinSynth == BC_WINDOW_CMD_FRAMES);
  assert(bcState.entry[BC_DOOR_FL].phase == BC_ENTRY_OPENED);
  httpBodyComfortConfig();
  assert(server.lastBody.find("synth 5 · SENT") != std::string::npos);
  // Restore also goes out without stock frames.
  g_tx.clear();
  car.winPosState[BC_DOOR_FL] = BC_WINPOS_FULL; car.winPos[BC_DOOR_FL] = 200;
  run(500);
  car.latch[BC_DOOR_FL] = 2;
  run(2500);
  w = takeTx(0x294);
  assert(!w.empty() && bcGetBitsPure(w[0].data, 52, 3) == BC_WINREQ_CLOSED);
  // Never seen: nothing sent, outcome recorded, entry released.
  resetAll();
  setCfg(c);
  car.vc3Mode = 2;
  run(2000);
  car.latch[BC_DOOR_FL] = 1;
  run(2500);
  assert(takeTx(0x294).empty());
  assert(bcState.lastWinOutcome == BC_WIN_OUTCOME_NO_0x294 && bcState.winCmdTimeouts == 1);
  assert(bcState.entry[BC_DOOR_FL].phase == BC_ENTRY_IDLE);
  httpBodyComfortConfig();
  assert(server.lastBody.find("\"vc3\":\"never seen\"") != std::string::npos);
  // Alternative open command: GOTO_PERCENT 100.
  resetAll();
  c.eeOpenPercent = true;
  setCfg(c);
  run(2000);
  car.latch[BC_DOOR_RR] = 1;
  run(800);
  w = takeTx(0x294);
  assert(!w.empty() && bcGetBitsPure(w[0].data, 52, 3) == BC_WINREQ_PERCENT);
  assert(bcGetBitsPure(w[0].data, 42, 6) == 20u && bcGetBitsPure(w[0].data, 48, 4) == 8u);
  server.args = {{"eeOpenPercent", "0"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 200 && !bcCfg.eeOpenPercent);
}

static void testRearLightReinforcement() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.rvLight = true;
  c.rvLightMode = BC_REAR_LIGHT_BOTH;
  setCfg(c);
  car.honk = true;       // a stale one-shot request in the stock frame
  run(500);
  assert(g_tx.empty());
  car.gear = BC_GEAR_R;
  run(300);
  g_tx.clear();
  run(1000);
  auto fog = takeTx(0x273);
  auto lights = takeTx(0x3B3);
  // 10 stock-follow copies plus reinforcement in between (every 40 ms).
  assert(fog.size() >= 20 && lights.size() >= 20);
  int reinforced = 0;
  for (const auto &f : fog) {
    assert(bcGetBitsPure(f.data, 23, 1) == 1u);
    if (bcGetBitsPure(f.data, 61, 1) == 0u) reinforced++;  // honk cleared only in rebuilt copies
  }
  assert(reinforced >= 10);
  for (const auto &f : lights) assert(bcGetBitsPure(f.data, 9, 3) == BC_LIGHT_SWITCH_ON);
  assert(bcState.rearLightReinforced > 0);
  // Stock frames stop: reinforcement stops once the template is 300 ms old.
  // Leaving R stops everything.
  car.gear = BC_GEAR_P;
  run(300);
  g_tx.clear();
  run(1000);
  assert(takeTx(0x273).empty() && takeTx(0x3B3).empty());
}

static void testWindowCaptureAndManualTest() {
  resetAll();
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  setCfg(c);
  run(500);
  // Touchscreen sends a VENT for all four windows once.
  uint8_t d[8] = {};
  d[0] = 0x40;
  bcComposeWindowRequestPure(d, 0x0Fu, BC_WINREQ_VENT, 0);
  bodyComfortObserve(BC_BUS_A, 0x294, d, 8);
  bodyComfortObserve(BC_BUS_A, 0x294, d, 8);  // repeated frame is not duplicated
  httpBodyComfortConfig();
  assert(server.lastBody.find("\"vc3Seen\":\"VENT FL FR RL RR pct 0 · ") != std::string::npos);
  assert(bcState.vc3SeenCount == 1 && bcState.vc3WindowFrames == 2);
  // Manual test: open RL in P.
  g_tx.clear();
  server.args = {{"door", "2"}, {"req", "4"}};
  httpBodyComfortWindowTest();
  assert(server.lastCode == 200);
  run(700);
  auto w = takeTx(0x294);
  assert(w.size() == BC_WINDOW_CMD_FRAMES && bcGetBitsPure(w[0].data, 48, 4) == 0x4u);
  assert(bcGetBitsPure(w[0].data, 52, 3) == BC_WINREQ_OPEN);
  assert(bcState.lastWinOutcome == BC_WIN_OUTCOME_SENT);
  // Validation and gating.
  server.args = {{"door", "5"}, {"req", "4"}};
  httpBodyComfortWindowTest();
  assert(server.lastCode == 400);
  server.args = {{"door", "0"}, {"req", "1"}};
  httpBodyComfortWindowTest();
  assert(server.lastCode == 400);
  car.gear = BC_GEAR_D;
  run(300);
  server.args = {{"door", "0"}, {"req", "3"}};
  httpBodyComfortWindowTest();
  assert(server.lastCode == 409);
  // Default rear-light mode is Lights ON.
  assert(bodyComfortDefaultsPure().rvLightMode == BC_REAR_LIGHT_LIGHTS_ON);
}

int main() {
  bodyComfortStart();
  assert(bodyComfortTaskHandle != nullptr);
  testUnsupportedTopologies();
  testAllOffByDefault();
  testEasyEntryOpenAndRestore();
  testEasyEntryBlocked();
  testMirrorFoldCameraUnfold();
  testMirrorDriverDoorUnfold();
  testMirrorCooldown();
  testSeatbelt();
  testReverseHazardAndLight();
  testDoorHazard();
  testHazardBlinkAndMacros();
  testHazardPressReleasedOnConfirm();
  testHttp();
  testExistingFeatureMacros();
  testKickdown();
  testDoorHandleFrunk();
  testListenOnly();
  testWindowDelivery();
  testRearLightReinforcement();
  testWindowCaptureAndManualTest();
  std::puts("body_comfort_runtime host: all tests passed");
  return 0;
}
