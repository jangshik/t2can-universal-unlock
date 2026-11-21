// Host regression suite for body_comfort_pure.h.
// Build: g++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_body_comfort_pure.cpp
#include <cassert>
#include <cstdio>
#include <cstring>
#include "body_comfort_pure.h"

static void testBitHelpers() {
  uint8_t d[8] = {};
  bcSetBitsPure(d, 52, 3, 5u);
  assert(bcGetBitsPure(d, 52, 3) == 5u);
  bcSetBitsPure(d, 3, 1, 1u);
  assert(d[0] == 0x08u);
  bcSetBitsPure(d, 3, 1, 0u);
  assert(d[0] == 0x00u);
  bcSetBitsPure(d, 60, 8, 0xFFu);  // out of range: ignored
  assert(bcGetBitsPure(d, 52, 3) == 5u);
  assert(bcSigned9Pure(0x1FFu) == -1);
  assert(bcSigned9Pure(0x0FFu) == 255);
  assert(bcSigned9Pure(0x100u) == -256);
}

static void testDoorDecode() {
  uint8_t d[8] = {};
  bcSetBitsPure(d, 14, 4, 2u);   // front CLOSED
  bcSetBitsPure(d, 18, 4, 5u);   // rear AJAR
  bcSetBitsPure(d, 52, 3, 1u);   // mirror FOLDED
  BcDoorFramePure f = {};
  assert(bcDecodeDoorFramePure(d, 8, f));
  assert(f.frontState == BC_DOOR_STATE_CLOSED);
  assert(f.rearState == BC_DOOR_STATE_OPEN);
  assert(f.mirrorFold == BC_MIRROR_FOLDED);
  assert(!bcDecodeDoorFramePure(d, 7, f));
  assert(bcDoorStateFromLatchPure(0) == BC_DOOR_STATE_UNKNOWN);
  assert(bcDoorStateFromLatchPure(3) == BC_DOOR_STATE_OPEN);   // CLOSING is not latched
  assert(bcDoorStateFromLatchPure(8) == BC_DOOR_STATE_UNKNOWN);
}

static void testWindowDecodeLearnRestore() {
  uint8_t m0[8] = {};
  bcSetBitsPure(m0, 0, 1, 0u);
  bcSetBitsPure(m0, 1, 4, BC_WINDOW_STOPPED);
  bcSetBitsPure(m0, 22, 9, (uint32_t)(-200) & 0x1FFu);
  bcSetBitsPure(m0, 33, 4, 7u);  // LR moving auto down
  bcSetBitsPure(m0, 54, 9, 100u);
  BcWindowFramePure w = {};
  assert(bcDecodeWindowFramePure(m0, 8, w));
  assert(w.mux == 0u && w.motionA == BC_WINDOW_STOPPED && w.posRawA == -200);
  assert(w.motionB == 7u && w.posRawB == 100);
  uint8_t m1[8] = {};
  bcSetBitsPure(m1, 0, 1, 1u);
  bcSetBitsPure(m1, 49, 3, BC_WINPOS_VENT);
  bcSetBitsPure(m1, 52, 3, BC_WINPOS_FULL);
  assert(bcDecodeWindowFramePure(m1, 8, w));
  assert(w.mux == 1u && w.posStateA == BC_WINPOS_VENT && w.posStateB == BC_WINPOS_FULL);

  BcWindowLearnPure learn = {};
  uint8_t pct = 0;
  assert(!bcWindowPercentOpenPure(learn, 0, pct));
  bcWindowLearnPure(learn, 2u /*moving*/, BC_WINPOS_CLOSED, -200);
  assert(!learn.closedValid);
  bcWindowLearnPure(learn, BC_WINDOW_STOPPED, BC_WINPOS_CLOSED, -200);
  bcWindowLearnPure(learn, BC_WINDOW_STOPPED, BC_WINPOS_FULL, 200);
  assert(learn.closedValid && learn.openValid);
  assert(bcWindowPercentOpenPure(learn, 0, pct) && pct == 50u);
  assert(bcWindowPercentOpenPure(learn, -200, pct) && pct == 0u);
  assert(bcWindowPercentOpenPure(learn, 250, pct) && pct == 100u);
  assert(bcWindowPercentOpenPure(learn, -160, pct) && pct == 10u);
  assert(bcWindowPercentOpenPure(learn, -150, pct) && pct == 15u);  // 12.5 % rounds up
  // Reversed travel direction is handled by the signed span.
  BcWindowLearnPure rev = {true, 200, true, -200};
  assert(bcWindowPercentOpenPure(rev, 100, pct) && pct == 25u);
  BcWindowLearnPure tiny = {true, 0, true, 10};
  assert(!bcWindowPercentOpenPure(tiny, 5, pct));

  BcRestorePlanPure p = bcRestorePlanPure(BC_WINPOS_CLOSED, false, 0);
  assert(p.valid && p.needed && p.request == BC_WINREQ_CLOSED);
  p = bcRestorePlanPure(BC_WINPOS_CLOSED_TRIM_CLEAR, false, 0);
  assert(p.valid && p.needed && p.request == BC_WINREQ_CLOSED);
  p = bcRestorePlanPure(BC_WINPOS_VENT, false, 0);
  assert(p.request == BC_WINREQ_VENT);
  p = bcRestorePlanPure(BC_WINPOS_PARTIAL, true, 35);
  assert(p.needed && p.request == BC_WINREQ_PERCENT && p.percent == 35u);
  p = bcRestorePlanPure(BC_WINPOS_PARTIAL, false, 0);
  assert(p.needed && p.request == BC_WINREQ_VENT);
  p = bcRestorePlanPure(BC_WINPOS_CRACKED, true, 0);
  assert(p.request == BC_WINREQ_CLOSED);
  p = bcRestorePlanPure(BC_WINPOS_FULL, true, 100);
  assert(p.valid && !p.needed);
  p = bcRestorePlanPure(BC_WINPOS_UNKNOWN, true, 50);
  assert(!p.valid);
}

static void testWindowCompose() {
  uint8_t d[8];
  memset(d, 0xFF, sizeof(d));  // every other field must survive
  assert(bcComposeWindowRequestPure(d, (1u << BC_DOOR_FR) | (1u << BC_DOOR_RL), BC_WINREQ_PERCENT, 35));
  assert(bcGetBitsPure(d, 42, 6) == 7u);
  assert(bcGetBitsPure(d, 48, 1) == 0u && bcGetBitsPure(d, 49, 1) == 1u);
  assert(bcGetBitsPure(d, 50, 1) == 1u && bcGetBitsPure(d, 51, 1) == 0u);
  assert(bcGetBitsPure(d, 52, 3) == BC_WINREQ_PERCENT);
  for (uint8_t bit = 0; bit < 42; ++bit) assert(bcGetBitsPure(d, bit, 1) == 1u);
  for (uint8_t bit = 55; bit < 64; ++bit) assert(bcGetBitsPure(d, bit, 1) == 1u);
  assert(bcComposeWindowRequestPure(d, 1u, BC_WINREQ_OPEN, 0));
  assert(bcGetBitsPure(d, 42, 6) == 0u && bcGetBitsPure(d, 52, 3) == BC_WINREQ_OPEN);
  assert(!bcComposeWindowRequestPure(d, 0u, BC_WINREQ_OPEN, 0));
  assert(!bcComposeWindowRequestPure(d, 1u, 5u, 0));
}

static void testGearSpeedWeatherLighting() {
  uint8_t d[8] = {};
  bcSetBitsPure(d, 21, 3, 2u);
  assert(bcDecodeGearPure(d, 8) == BC_GEAR_R);
  bcSetBitsPure(d, 21, 3, 7u);
  assert(bcDecodeGearPure(d, 8) == BC_GEAR_INVALID);

  uint8_t s[8] = {};
  int32_t kph10 = 0;
  bcSetBitsPure(s, 43, 13, 500u);  // 500*0.08-40 = 0 kph
  assert(bcDecodeSpeedPure(s, 8, kph10) && kph10 == 0);
  bcSetBitsPure(s, 43, 13, 625u);  // 10 kph
  assert(bcDecodeSpeedPure(s, 8, kph10) && kph10 == 100);
  bcSetBitsPure(s, 43, 13, 8191u);
  assert(!bcDecodeSpeedPure(s, 8, kph10));

  uint8_t e[8] = {0x02};
  bool rain = true, snow = false;
  assert(bcDecodeWeatherPure(e, 8, rain, snow) && !rain && snow);

  uint8_t l[8] = {};
  bcSetBitsPure(l, 4, 4, 1u);
  bcSetBitsPure(l, 0, 2, 2u);
  uint8_t hz = 0; bool ind = false;
  assert(bcDecodeLightingPure(l, 8, hz, ind) && hz == BC_HAZARD_REQ_BUTTON && ind);

  uint8_t u[8] = {};
  bool cam = true;
  assert(bcDecodeCameraActivePure(u, 8, cam) && !cam);
  bcSetBitsPure(u, 41, 1, 1u);
  assert(bcDecodeCameraActivePure(u, 8, cam) && cam);
}

static void testSwitchFields() {
  uint8_t m1[8] = {};
  bcSetBitsPure(m1, 0, 1, 1u);
  for (uint8_t b = 0; b < BC_BTN_COUNT; ++b) bcSetBitsPure(m1, bcButtonStartBitPure(b), 2, 1u);
  for (uint8_t b = 0; b < BC_BTN_COUNT; ++b) assert(!bcButtonPressedPure(m1, b));
  bcSetBitsPure(m1, 12, 2, BC_SWC_ON);
  assert(bcButtonPressedPure(m1, BC_BTN_RIGHT_PRESS));
  assert(!bcButtonPressedPure(m1, BC_BTN_LEFT_PRESS));
  assert(!bcCameraButtonPressedPure(m1));
  bcSetCameraButtonPure(m1, true);
  assert(bcCameraButtonPressedPure(m1));
  bcSetCameraButtonPure(m1, false);
  assert(bcGetBitsPure(m1, 50, 2) == 1u);
  assert(bcSwitchMuxPure(m1) == 1u);
  // Voice (48|2) and camera (50|2) are macro buttons too.
  assert(bcButtonStartBitPure(BC_BTN_VOICE) == 48u && bcButtonStartBitPure(BC_BTN_CAMERA) == 50u);
  bcSetBitsPure(m1, 48, 2, BC_SWC_ON);
  assert(bcButtonPressedPure(m1, BC_BTN_VOICE));
  bcSetCameraButtonPure(m1, true);
  assert(bcButtonPressedPure(m1, BC_BTN_CAMERA));
  bcSetBitsPure(m1, 50, 2, 1u);  // SOFT/OFF is not a press
  assert(!bcButtonPressedPure(m1, BC_BTN_CAMERA));

  uint8_t m0[8] = {};
  assert(!bcHazardButtonPressedPure(m0));
  bcSetHazardButtonPure(m0, true);
  assert(bcHazardButtonPressedPure(m0) && m0[0] == 0x08u);
  bcSetBitsPure(m0, 40, 1, 1u);  // RF switch-pack bit
  assert(bcWindowSwitchActivePure(m0, BC_DOOR_FR));
  assert(!bcWindowSwitchActivePure(m0, BC_DOOR_FL));
  assert(!bcWindowSwitchActivePure(m0, BC_DOOR_RR));
}

static void make3A1(uint8_t d[8], uint8_t left, uint8_t center, uint8_t right) {
  memset(d, 0, 8);
  d[0] = 0x00;  // mux0
  d[1] = 0x5Au;
  bcSetBitsPure(d, 36, 2, left);
  bcSetBitsPure(d, 38, 2, center);
  bcSetBitsPure(d, 40, 2, right);
  bcSetBitsPure(d, 52, 4, 9u);
  d[7] = teslaAdditiveChecksumPure(0x3A1u, d);
}

static void testSeatbelt() {
  uint8_t in[8], out[8];
  make3A1(in, 1, 1, 1);
  assert(bcSeatbeltTransformPure(in, 8, 0x05u, out) == BC_SEATBELT_CHANGED);
  assert(bcGetBitsPure(out, 36, 2) == 0u);
  assert(bcGetBitsPure(out, 38, 2) == 1u);  // centre not selected
  assert(bcGetBitsPure(out, 40, 2) == 0u);
  assert(bcGetBitsPure(out, 52, 4) == 9u);  // counter preserved
  assert(out[1] == 0x5Au);
  assert(out[7] == teslaAdditiveChecksumPure(0x3A1u, out));
  make3A1(in, 0, 2, 0);
  assert(bcSeatbeltTransformPure(in, 8, 0x07u, out) == BC_SEATBELT_UNCHANGED);  // SNA untouched
  make3A1(in, 1, 0, 0);
  in[7] ^= 0x01u;
  assert(bcSeatbeltTransformPure(in, 8, 0x07u, out) == BC_SEATBELT_BAD_CHECKSUM);
  make3A1(in, 1, 0, 0);
  in[0] = 0x01u;  // mux1
  assert(bcSeatbeltTransformPure(in, 8, 0x07u, out) == BC_SEATBELT_NOT_MUX0);
}

static void testGestures() {
  const uint16_t longMs = 800, gapMs = 400;
  BcGesturePure g = {};
  uint32_t t = 1000;
  assert(bcGestureObservePure(g, false, t, longMs) == BC_GESTURE_NONE);
  // Double click.
  auto frame = [&](bool p) { uint8_t e = bcGestureObservePure(g, p, t, longMs); t += 50; return e; };
  assert(frame(true) == BC_GESTURE_NONE);
  assert(frame(false) == BC_GESTURE_NONE);
  assert(frame(true) == BC_GESTURE_NONE);
  assert(frame(false) == BC_GESTURE_NONE);
  assert(bcGestureTickPure(g, t + 100, gapMs, 600) == BC_GESTURE_NONE);  // still in gap
  g.lastObsMs = t + 400;
  assert(bcGestureTickPure(g, t + 450, gapMs, 600) == BC_GESTURE_DOUBLE);
  // Triple click.
  t += 1000;
  frame(false);
  for (int i = 0; i < 3; ++i) { frame(true); frame(false); }
  g.lastObsMs = t + 400;
  assert(bcGestureTickPure(g, t + 450, gapMs, 600) == BC_GESTURE_TRIPLE);
  // Single click is ignored.
  t += 1000;
  frame(false); frame(true); frame(false);
  g.lastObsMs = t + 400;
  assert(bcGestureTickPure(g, t + 450, gapMs, 600) == BC_GESTURE_NONE);
  // Long press fires once while held, and its release is not a click.
  t += 1000;
  frame(false);
  assert(frame(true) == BC_GESTURE_NONE);
  uint8_t ev = BC_GESTURE_NONE;
  for (int i = 0; i < 20 && ev == BC_GESTURE_NONE; ++i) ev = frame(true);
  assert(ev == BC_GESTURE_LONG);
  assert(frame(true) == BC_GESTURE_NONE);
  assert(frame(false) == BC_GESTURE_NONE);
  g.lastObsMs = t + 400;
  assert(bcGestureTickPure(g, t + 450, gapMs, 600) == BC_GESTURE_NONE);
  // Button already held when first observed never produces a long press.
  BcGesturePure h = {};
  uint32_t u = 0;
  bcGestureObservePure(h, true, u, longMs);
  for (int i = 0; i < 40; ++i) assert(bcGestureObservePure(h, true, u += 50, longMs) == BC_GESTURE_NONE);
  // Stale input resets the detector.
  BcGesturePure s = {};
  bcGestureObservePure(s, false, 0, longMs);
  bcGestureObservePure(s, true, 50, longMs);
  bcGestureObservePure(s, false, 100, longMs);
  bcGestureObservePure(s, true, 150, longMs);
  bcGestureObservePure(s, false, 200, longMs);
  assert(bcGestureTickPure(s, 900, gapMs, 600) == BC_GESTURE_NONE);
  assert(!s.valid);
  // uint32 wrap.
  BcGesturePure w = {};
  uint32_t x = 0xFFFFFF00u;
  bcGestureObservePure(w, false, x, longMs);
  bcGestureObservePure(w, true, x += 50, longMs);
  bcGestureObservePure(w, false, x += 50, longMs);
  bcGestureObservePure(w, true, x += 50, longMs);
  bcGestureObservePure(w, false, x += 50, longMs);
  w.lastObsMs = x + 400;
  assert(bcGestureTickPure(w, x + 450, gapMs, 600) == BC_GESTURE_DOUBLE);
}

static void testHazardController() {
  BcHazardCtlPure c = {};
  uint32_t t = 0;
  // Desired on, hazards off -> press once, wait for settle.
  assert(bcHazardStepPure(c, t, true, BC_HAZARD_REQ_NONE, true) == BC_HAZARD_ACTION_PRESS);
  assert(bcHazardStepPure(c, t += 500, true, BC_HAZARD_REQ_BUTTON, true) == BC_HAZARD_ACTION_NONE);
  assert(bcHazardStepPure(c, t += 800, true, BC_HAZARD_REQ_BUTTON, true) == BC_HAZARD_ACTION_NONE);
  assert(c.ownedOn);
  // Request ends -> press to turn off what we turned on.
  assert(bcHazardStepPure(c, t += 100, true, BC_HAZARD_REQ_BUTTON, false) == BC_HAZARD_ACTION_PRESS);
  assert(bcHazardStepPure(c, t += 1300, true, BC_HAZARD_REQ_NONE, false) == BC_HAZARD_ACTION_NONE);
  assert(!c.ownedOn);

  // Hazards already on by the driver: never touched.
  BcHazardCtlPure d = {};
  assert(bcHazardStepPure(d, 0, true, BC_HAZARD_REQ_BUTTON, true) == BC_HAZARD_ACTION_NONE);
  assert(bcHazardStepPure(d, 100, true, BC_HAZARD_REQ_BUTTON, false) == BC_HAZARD_ACTION_NONE);

  // Driver cancels our hazards while the request is active: back off.
  BcHazardCtlPure e = {};
  t = 0;
  assert(bcHazardStepPure(e, t, true, 0, true) == BC_HAZARD_ACTION_PRESS);
  assert(bcHazardStepPure(e, t += 1300, true, 1, true) == BC_HAZARD_ACTION_NONE);
  assert(e.ownedOn);
  assert(bcHazardStepPure(e, t += 100, true, 0, true) == BC_HAZARD_ACTION_NONE);
  assert(e.userOverride && !e.ownedOn);
  assert(bcHazardStepPure(e, t += 2000, true, 0, true) == BC_HAZARD_ACTION_NONE);
  // A new request episode presses again.
  assert(bcHazardStepPure(e, t += 100, true, 0, false) == BC_HAZARD_ACTION_NONE);
  assert(bcHazardStepPure(e, t += 100, true, 0, true) == BC_HAZARD_ACTION_PRESS);

  // Lock flash (another source) blocks the press until NONE.
  BcHazardCtlPure f = {};
  assert(bcHazardStepPure(f, 0, true, 2, true) == BC_HAZARD_ACTION_NONE);
  assert(bcHazardStepPure(f, 100, false, 0, true) == BC_HAZARD_ACTION_NONE);
  assert(bcHazardStepPure(f, 200, true, 0, true) == BC_HAZARD_ACTION_PRESS);

  // Press without effect retries at most three times.
  BcHazardCtlPure r = {};
  t = 0;
  int presses = 0;
  for (int i = 0; i < 40; ++i, t += 200)
    presses += bcHazardStepPure(r, t, true, 0, true) == BC_HAZARD_ACTION_PRESS;
  assert(presses == BC_HAZARD_MAX_ATTEMPTS);
}

static void testDoorHazard() {
  BcDoorHazardPure s = {};
  assert(!bcDoorHazardStepPure(s, 0, true, false, true, true, 2000));   // P: no episode
  assert(bcDoorHazardStepPure(s, 10, true, true, true, true, 2000));    // D + door open
  assert(bcDoorHazardStepPure(s, 20, true, false, true, true, 2000));   // stays while open in P
  assert(bcDoorHazardStepPure(s, 100, true, true, true, false, 2000));  // closed, delay
  assert(bcDoorHazardStepPure(s, 2099, true, true, true, false, 2000));
  assert(!bcDoorHazardStepPure(s, 2100, true, true, true, false, 2000));
  // Re-open during delay restarts it.
  assert(bcDoorHazardStepPure(s, 3000, true, true, true, true, 2000));
  assert(bcDoorHazardStepPure(s, 3100, true, true, true, false, 2000));
  assert(bcDoorHazardStepPure(s, 4000, true, true, true, true, 2000));
  assert(bcDoorHazardStepPure(s, 4100, true, true, true, false, 2000));
  assert(bcDoorHazardStepPure(s, 6000, true, true, true, false, 2000));
  assert(!bcDoorHazardStepPure(s, 6100, true, true, true, false, 2000));
  // Lost door data ends the episode after the delay; disabled clears it.
  assert(bcDoorHazardStepPure(s, 7000, true, true, true, true, 2000));
  assert(bcDoorHazardStepPure(s, 7100, true, true, false, false, 2000));
  assert(!bcDoorHazardStepPure(s, 9100, true, true, false, false, 2000));
  assert(bcDoorHazardStepPure(s, 9200, true, true, true, true, 2000));
  assert(!bcDoorHazardStepPure(s, 9300, false, true, true, true, 2000));
}

static void testUnfold() {
  BcUnfoldTimerPure t = {};
  assert(!bcUnfoldDuePure(t, 0, true, true, 100, 10, 3));     // exactly 10 kph: not above
  assert(!bcUnfoldDuePure(t, 100, true, true, 101, 10, 3));
  assert(!bcUnfoldDuePure(t, 3099, true, true, 150, 10, 3));
  assert(bcUnfoldDuePure(t, 3100, true, true, 150, 10, 3));
  assert(!bcUnfoldDuePure(t, 3200, true, true, 50, 10, 3));   // dropped: restart
  assert(!bcUnfoldDuePure(t, 3300, true, true, 150, 10, 3));
  assert(!bcUnfoldDuePure(t, 9000, false, true, 150, 10, 3));  // not D
  assert(!bcUnfoldDuePure(t, 9100, true, false, 150, 10, 3));  // stale speed
  assert(bcUnfoldDuePure(t, 9200, true, true, 150, 10, 0));    // hold 0 s
}

static void testConfigBlob() {
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  assert(!c.eeEnabled && !c.sbEnabled && !c.mcEnabled && !c.hzBlinkEnabled);
  assert(!c.rvHazard && !c.rvLight && !c.drHazard);
  c.eeEnabled = true;
  c.eeDoorMask = 0x05;
  c.eeRestoreDelayMs = 1234;
  c.eeMirrorFold = true;
  c.sbEnabled = true;
  c.sbSeatMask = 0x02;
  c.mcEnabled = true;
  c.mcAction[BC_BTN_RIGHT_PRESS][BC_GESTURE_LONG] = BC_ACTION_RUN_HAZARD_BLINK;
  c.mcAction[BC_BTN_LEFT_TILT_RIGHT][BC_GESTURE_TRIPLE] = BC_ACTION_TOGGLE_DOOR_HAZARD;
  c.mcAction[BC_BTN_VOICE][BC_GESTURE_DOUBLE] = BC_ACTION_TOGGLE_EASY_ENTRY;
  c.mcAction[BC_BTN_CAMERA][BC_GESTURE_LONG] = BC_ACTION_MIRROR_FOLD_NOW;
  c.rvLight = true;
  c.rvLightMode = BC_REAR_LIGHT_FOG;
  c.drOffDelayMs = 4500;
  uint8_t blob[BC_CFG_BLOB_LEN];
  bodyComfortSerializePure(c, blob);
  BodyComfortConfigPure r = {};
  assert(bodyComfortDeserializePure(blob, BC_CFG_BLOB_LEN, r));
  assert(memcmp(&r.mcAction, &c.mcAction, sizeof(c.mcAction)) == 0);
  assert(r.eeEnabled && r.eeDoorMask == 0x05 && r.eeRestoreDelayMs == 1234 && r.eeMirrorFold);
  assert(r.sbEnabled && r.sbSeatMask == 0x02 && r.mcEnabled && r.rvLight);
  assert(r.rvLightMode == BC_REAR_LIGHT_FOG && r.drOffDelayMs == 4500 && r.eeWeatherBlock);
  assert(BC_CFG_BLOB_LEN == 50u && blob[0] == 3u);
  // v1 (36-byte, six wheel controls) blobs still load; voice/camera start empty.
  uint8_t v1[BC_CFG_BLOB_LEN_V1];
  memcpy(v1, blob, BC_CFG_BLOB_LEN_V1 - 1u);
  v1[0] = 1u;
  v1[BC_CFG_BLOB_LEN_V1 - 1u] = bcCfgBlobChecksumLenPure(v1, BC_CFG_BLOB_LEN_V1);
  BodyComfortConfigPure m = {};
  assert(bodyComfortDeserializePure(v1, BC_CFG_BLOB_LEN_V1, m));
  assert(m.eeEnabled && m.eeRestoreDelayMs == 1234);
  assert(m.mcAction[BC_BTN_RIGHT_PRESS][BC_GESTURE_LONG] == BC_ACTION_RUN_HAZARD_BLINK);
  assert(m.mcAction[BC_BTN_VOICE][BC_GESTURE_DOUBLE] == BC_ACTION_NONE);
  assert(m.mcAction[BC_BTN_CAMERA][BC_GESTURE_LONG] == BC_ACTION_NONE);
  v1[0] = 2u;  // wrong version for the length
  v1[BC_CFG_BLOB_LEN_V1 - 1u] = bcCfgBlobChecksumLenPure(v1, BC_CFG_BLOB_LEN_V1);
  assert(!bodyComfortDeserializePure(v1, BC_CFG_BLOB_LEN_V1, m));
  blob[5] ^= 0x10u;
  assert(!bodyComfortDeserializePure(blob, BC_CFG_BLOB_LEN, r));
  assert(!r.eeEnabled);  // defaults on corruption
  assert(!bodyComfortDeserializePure(blob, BC_CFG_BLOB_LEN - 1u, r));

  BodyComfortConfigPure bad = bodyComfortDefaultsPure();
  bad.eeRestoreDelayMs = 60000; bad.eeUnfoldKph = 0; bad.mcLongMs = 10; bad.mcGapMs = 9000;
  bad.hzBlinkSeconds = 0; bad.rvLightMode = 9; bad.eeDoorMask = 0xFF; bad.mcAction[0][0] = 200;
  bodyComfortSanitizePure(bad);
  assert(bad.eeRestoreDelayMs == 5000 && bad.eeUnfoldKph == 1 && bad.mcLongMs == 500);
  assert(bad.mcGapMs == 800 && bad.hzBlinkSeconds == 1 && bad.rvLightMode == BC_REAR_LIGHT_BOTH);
  assert(bad.eeDoorMask == 0x0F && bad.mcAction[0][0] == BC_ACTION_NONE);

  BodyComfortConfigPure t = bodyComfortDefaultsPure();
  assert(bodyComfortApplyToggleActionPure(t, BC_ACTION_TOGGLE_EASY_ENTRY) && t.eeEnabled);
  assert(bodyComfortApplyToggleActionPure(t, BC_ACTION_TOGGLE_EASY_ENTRY) && !t.eeEnabled);
  assert(!bodyComfortApplyToggleActionPure(t, BC_ACTION_RUN_HAZARD_BLINK));
  assert(!bodyComfortApplyToggleActionPure(t, BC_ACTION_RETIRED_4));
  assert(!bodyComfortApplyToggleActionPure(t, BC_ACTION_TOGGLE_NAG_KILLER));  // runtime-owned
  assert(bcActionIsExistingFeaturePure(BC_ACTION_TOGGLE_NAG_KILLER));
  assert(bcActionIsExistingFeaturePure(BC_ACTION_TOGGLE_BATTERY_PREHEAT));
  assert(!bcActionIsExistingFeaturePure(BC_ACTION_CAMERA));
  assert(BC_ACTION_COUNT == 23u);
  assert(bcActionIsExistingFeaturePure(BC_ACTION_TOGGLE_LISTEN_ONLY));
  assert(!bcActionIsExistingFeaturePure(BC_ACTION_OPEN_FRUNK));
  assert(!bcActionIsExistingFeaturePure(BC_ACTION_TOGGLE_KICKDOWN));
  assert(bodyComfortApplyToggleActionPure(t, BC_ACTION_TOGGLE_KICKDOWN) && t.kdEnabled);
  // The retired "toggle hazard blink" slot (4) loads as NONE.
  BodyComfortConfigPure retired = bodyComfortDefaultsPure();
  retired.mcAction[2][1] = BC_ACTION_RETIRED_4;
  retired.mcAction[3][0] = BC_ACTION_TOGGLE_TLSSC;
  bodyComfortSanitizePure(retired);
  assert(retired.mcAction[2][1] == BC_ACTION_NONE && retired.mcAction[3][0] == BC_ACTION_TOGGLE_TLSSC);
  assert(!bodyComfortApplyToggleActionPure(t, BC_ACTION_NONE));
}

static void testWifiTiming() {
  WifiApTimingPure t = wifiApTimingDefaultsPure();
  assert(wifiApStartDelayMsPure(t, false) == 0u);
  t.slowStartEnabled = true;
  t.startDelayS = 45;
  assert(wifiApStartDelayMsPure(t, false) == 45000u);
  assert(wifiApStartDelayMsPure(t, true) == 0u);  // setup mode never waits
  t.startDelayS = 9999;
  wifiApTimingSanitizePure(t);
  assert(t.startDelayS == 600u);
  t.hideEnabled = true;
  t.hideDelayS = 10;
  assert(!wifiApHideDuePure(t, false, false, 50000, 0));
  assert(!wifiApHideDuePure(t, false, true, 9999, 0));
  assert(wifiApHideDuePure(t, false, true, 10000, 0));
  assert(!wifiApHideDuePure(t, true, true, 10000, 0));
  assert(!wifiApHideDuePure(t, false, true, 5000u, 0xFFFFF000u));  // 9.1 s across wrap
  assert(wifiApHideDuePure(t, false, true, 6000u, 0xFFFFF000u));   // 10.1 s across wrap
}

static void testKickdownPureAndV3() {
  // Decoders.
  uint8_t d[8] = {};
  uint16_t pct = 0; bool brake = true;
  bcSetBitsPure(d, 32, 8, 225u);
  assert(bcDecodeAccelPedalPure(d, 8, pct) && pct == 900u);
  bcSetBitsPure(d, 32, 8, 255u);
  assert(!bcDecodeAccelPedalPure(d, 8, pct));
  assert(bcDecodeBrakePure(d, 8, brake) && !brake);
  bcSetBitsPure(d, 19, 2, 1u);
  assert(bcDecodeBrakePure(d, 8, brake) && brake);
  bcSetBitsPure(d, 19, 2, 2u);
  assert(!bcDecodeBrakePure(d, 8, brake));
  uint8_t door[8] = {};
  bcSetBitsPure(door, 10, 1, 1u);
  BcDoorFramePure f = {};
  assert(bcDecodeDoorFramePure(door, 8, f) && f.frontHandlePulled);
  uint8_t lk[8] = {};
  BcLockFramePure lock = {};
  assert(!bcDecodeLockPure(lk, 8, lock));  // SNA
  bcSetBitsPure(lk, 54, 2, 1u);
  bcSetBitsPure(lk, 21, 1, 1u);
  assert(bcDecodeLockPure(lk, 8, lock) && lock.simple == 1u && !lock.frontLocked[0] && lock.frontLocked[1]);
  assert(bcHandleMacroAllowedPure(true, lock, 0, BC_DOOR_STATE_OPEN));
  assert(!bcHandleMacroAllowedPure(true, lock, 1, BC_DOOR_STATE_OPEN));   // passenger door locked
  assert(!bcHandleMacroAllowedPure(true, lock, 0, BC_DOOR_STATE_CLOSED)); // door not open
  assert(!bcHandleMacroAllowedPure(false, lock, 0, BC_DOOR_STATE_OPEN));  // stale lock data
  bcSetBitsPure(lk, 54, 2, 2u);
  assert(bcDecodeLockPure(lk, 8, lock) && !bcHandleMacroAllowedPure(true, lock, 0, BC_DOOR_STATE_OPEN));
  uint8_t ui[8] = {};
  bcSetFrunkRequestPure(ui);
  assert(ui[0] == 0x20u);
  // Step function.
  BodyComfortConfigPure c = bodyComfortDefaultsPure();
  c.kdEnabled = true;
  BcKickdownPure k = {};
  BcKickdownInputPure in = {true, true, true, 900, true, false};
  assert(bcKickdownStepPure(k, 0, in, c) == BC_KD_ACTION_NONE);
  assert(bcKickdownStepPure(k, 99, in, c) == BC_KD_ACTION_NONE);
  assert(bcKickdownStepPure(k, 100, in, c) == BC_KD_ACTION_ENGAGE && k.kicked);
  in.pedalPct10 = 200;
  assert(bcKickdownStepPure(k, 200, in, c) == BC_KD_ACTION_NONE);
  assert(bcKickdownStepPure(k, 700, in, c) == BC_KD_ACTION_RELEASE && !k.kicked);
  // Brake blocks engagement.
  in.pedalPct10 = 1000; in.brakeOn = true;
  assert(bcKickdownStepPure(k, 1000, in, c) == BC_KD_ACTION_NONE);
  assert(bcKickdownStepPure(k, 2000, in, c) == BC_KD_ACTION_NONE && !k.kicked);
  // Failure handling.
  in.brakeOn = false;
  bcKickdownStepPure(k, 3000, in, c);
  assert(bcKickdownStepPure(k, 3100, in, c) == BC_KD_ACTION_ENGAGE);
  bcKickdownRequestFailedPure(k, 3100, BC_KD_ACTION_ENGAGE, true);
  assert(!k.kicked && bcKickdownStepPure(k, 3300, in, c) == BC_KD_ACTION_NONE);
  bcKickdownStepPure(k, 3400, in, c);
  assert(bcKickdownStepPure(k, 3500, in, c) == BC_KD_ACTION_ENGAGE);
  in.gearD = false;
  assert(bcKickdownStepPure(k, 3600, in, c) == BC_KD_ACTION_RELEASE);
  bcKickdownRequestFailedPure(k, 3600, BC_KD_ACTION_RELEASE, false);
  assert(!k.kicked);  // no retry outside D
  // Sanitize keeps a release path and a gap below the trigger.
  BodyComfortConfigPure s = bodyComfortDefaultsPure();
  s.kdTriggerPct = 30; s.kdReleasePct = 50; s.kdReleaseOnPedal = false; s.kdReleaseOnBrake = false;
  s.kdTargetRaw = 7; s.kdReleaseMode = 9; s.dhDoorMask = 0xFF; s.dhHoldS = 0; s.dhAction = 4;
  bodyComfortSanitizePure(s);
  assert(s.kdReleasePct == 25 && s.kdReleaseOnPedal && s.kdTargetRaw == BC_KD_TARGET_STANDARD);
  assert(s.kdReleaseMode == 0 && s.dhDoorMask == 3 && s.dhHoldS == 1 && s.dhAction == BC_ACTION_NONE);
  // Config v3 round trip and v2 migration.
  BodyComfortConfigPure v = bodyComfortDefaultsPure();
  assert(v.dhAction == BC_ACTION_OPEN_FRUNK && v.dhDoorMask == 1 && v.kdTriggerPct == 90);
  v.kdEnabled = true; v.kdTriggerPct = 75; v.kdTargetRaw = 2; v.kdReleasePct = 15;
  v.kdReleaseOnBrake = false; v.kdReleaseMode = 1; v.dhEnabled = true; v.dhDoorMask = 3;
  v.dhHoldS = 4; v.dhAction = BC_ACTION_TOGGLE_TLSSC; v.mcAction[7][2] = BC_ACTION_OPEN_FRUNK;
  uint8_t blob[BC_CFG_BLOB_LEN];
  bodyComfortSerializePure(v, blob);
  BodyComfortConfigPure r = {};
  assert(bodyComfortDeserializePure(blob, BC_CFG_BLOB_LEN, r));
  assert(r.kdEnabled && r.kdTriggerPct == 75 && r.kdTargetRaw == 2 && r.kdReleasePct == 15);
  assert(!r.kdReleaseOnBrake && r.kdReleaseOnPedal && r.kdReleaseMode == 1);
  assert(r.dhEnabled && r.dhDoorMask == 3 && r.dhHoldS == 4 && r.dhAction == BC_ACTION_TOGGLE_TLSSC);
  assert(r.mcAction[7][2] == BC_ACTION_OPEN_FRUNK);
  uint8_t v2[BC_CFG_BLOB_LEN_V2];
  memcpy(v2, blob, BC_CFG_BLOB_LEN_V2 - 1u);
  v2[0] = 2u;
  v2[BC_CFG_BLOB_LEN_V2 - 1u] = bcCfgBlobChecksumLenPure(v2, BC_CFG_BLOB_LEN_V2);
  assert(bodyComfortDeserializePure(v2, BC_CFG_BLOB_LEN_V2, r));
  assert(r.mcAction[7][2] == BC_ACTION_OPEN_FRUNK && !r.kdEnabled && !r.dhEnabled);
  assert(r.dhAction == BC_ACTION_OPEN_FRUNK && r.kdTriggerPct == 90);
}

int main() {
  testBitHelpers();
  testDoorDecode();
  testWindowDecodeLearnRestore();
  testWindowCompose();
  testGearSpeedWeatherLighting();
  testSwitchFields();
  testSeatbelt();
  testGestures();
  testHazardController();
  testDoorHazard();
  testUnfold();
  testConfigBlob();
  testWifiTiming();
  testKickdownPureAndV3();
  std::puts("body_comfort_pure: all tests passed");
  return 0;
}
