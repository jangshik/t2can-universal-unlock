// Host harness for body_comfort_runtime.h: compiles the real runtime against
// minimal Arduino / FreeRTOS / CAN stubs and drives it with simulated Juniper
// Body CAN traffic. Build:
//   g++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_body_comfort_runtime_host.cpp
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ─── Arduino String stub ─────────────────────────────────────────────────
class String {
 public:
  std::string v;
  String() = default;
  String(const char *s) : v(s ? s : "") {}
  String(const std::string &s) : v(s) {}
  String(unsigned long x) : v(std::to_string(x)) {}
  String(long x) : v(std::to_string(x)) {}
  String(unsigned int x) : v(std::to_string(x)) {}
  String(int x) : v(std::to_string(x)) {}
  void reserve(size_t n) { v.reserve(n); }
  size_t length() const { return v.size(); }
  char charAt(size_t i) const { return i < v.size() ? v[i] : 0; }
  int indexOf(char c, int from = 0) const {
    const size_t p = v.find(c, (size_t)from);
    return p == std::string::npos ? -1 : (int)p;
  }
  String substring(int a, int b = -1) const {
    return String(b < 0 ? v.substr((size_t)a) : v.substr((size_t)a, (size_t)(b - a)));
  }
  long toInt() const { return v.empty() ? 0 : std::atol(v.c_str()); }
  const char *c_str() const { return v.c_str(); }
  bool operator==(const char *s) const { return v == s; }
  String &operator+=(const char *s) { v += (s ? s : ""); return *this; }
  String &operator+=(char c) { v += c; return *this; }
  String &operator+=(unsigned long x) { v += std::to_string(x); return *this; }
  String &operator+=(long x) { v += std::to_string(x); return *this; }
  String &operator+=(unsigned int x) { v += std::to_string(x); return *this; }
  String &operator+=(int x) { v += std::to_string(x); return *this; }
  String &operator+=(const String &s) { v += s.v; return *this; }
};
static String operator+(const String &a, const char *b) { String r = a; r += b; return r; }

// ─── Time / FreeRTOS stubs ───────────────────────────────────────────────
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
typedef void *TaskHandle_t;
typedef int BaseType_t;
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
static void vTaskDelay(uint32_t) {}
static BaseType_t xTaskCreatePinnedToCore(void (*)(void *), const char *, uint32_t, void *,
                                          int, TaskHandle_t *h, int) {
  static int dummy;
  *h = &dummy;
  return pdPASS;
}
#define T2CAN_SERIAL_PRINTF(...) ((void)0)
#define T2CAN_SERIAL_PRINTLN(...) ((void)0)

// ─── NVS stub ────────────────────────────────────────────────────────────
static std::map<std::string, std::vector<uint8_t>> g_nvs;
class Preferences {
 public:
  bool begin(const char *ns, bool) { ns_ = ns; return true; }
  void end() {}
  size_t putBytes(const char *k, const void *d, size_t n) {
    g_nvs[ns_ + "/" + k].assign((const uint8_t *)d, (const uint8_t *)d + n);
    return n;
  }
  size_t getBytesLength(const char *k) { auto it = g_nvs.find(ns_ + "/" + k); return it == g_nvs.end() ? 0 : it->second.size(); }
  size_t getBytes(const char *k, void *d, size_t n) {
    auto it = g_nvs.find(ns_ + "/" + k);
    if (it == g_nvs.end() || it->second.size() > n) return 0;
    memcpy(d, it->second.data(), it->second.size());
    return it->second.size();
  }
 private:
  std::string ns_;
};

// ─── Web server stub ─────────────────────────────────────────────────────
struct ServerStub {
  std::map<std::string, std::string> args;
  int lastCode = 0;
  std::string lastBody;
  bool hasArg(const char *k) const { return args.count(k) != 0; }
  String arg(const char *k) const { auto it = args.find(k); return String(it == args.end() ? "" : it->second); }
  void send(int code, const char *, const String &body) { lastCode = code; lastBody = body.v; }
  void send(int code, const char *, const char *body) { lastCode = code; lastBody = body; }
  void sendHeader(const char *, const char *) {}
};
static ServerStub server;

// ─── Vehicle profile runtime state ───────────────────────────────────────
#include "vehicle_profile.h"
static volatile uint8_t activeVehicleProfile = VEHICLE_MODEL_Y_JUNIPER;
static volatile uint8_t activeVehicleTopology = VEHICLE_TOPOLOGY_STANDARD_BODY_CHASSIS;
static volatile bool vehicleProfileSetupMode = false;
static volatile bool vehicleProfileNvsError = false;
static inline bool activeProfileIsYl() { return activeVehicleProfile == VEHICLE_MODEL_YL; }
static inline bool activeProfileIsModelS2017() { return activeVehicleProfile == VEHICLE_MODEL_S_2017; }
static inline bool activeCanAIsBody() {
  return vehicleProfileCanAIsBody(activeVehicleProfile, activeVehicleTopology);
}
static inline bool activeProfileUsesYlTopology() { return vehicleProfileUsesYlTopology(activeVehicleProfile); }

// ─── Existing-feature stand-ins (record what the macro dispatcher calls) ──
static bool g_supported = true, g_saveOk = true, g_quiesceOk = true;
static int g_adminHoldDepth = 0, g_nagCommits = 0;
static inline bool activeProfileNagSupported() { return g_supported; }
static inline bool activeProfileIsaSuppressionSupported() { return g_supported; }
static inline bool activeProfileDmsNagSupported() { return g_supported; }
static inline bool activeProfileAdvancedEapSupported() { return g_supported; }
static inline bool activeProfileBodyControlsSupported() { return g_supported; }
static inline bool activeProfilePreheatSupported() { return g_supported; }
struct NagConfig { bool enabled = false; };
static portMUX_TYPE nagCfgMux = 0;
static NagConfig nagCfg;
static bool nagCfgApplyActiveProfilePolicy(NagConfig &, bool) { return true; }
static void nagCfgClampAll(NagConfig &) {}
struct NagScrollStoredSnapshot { int v = 7; };
static NagScrollStoredSnapshot nagScrollRuntimeSnapshot() { return {}; }
static void nagScrollRuntimeRestore(const NagScrollStoredSnapshot &s) { assert(s.v == 7); }
static bool tsl9InputQuiesceForConfig() { return g_quiesceOk; }
static void setCanTxAdministrativeHold(bool hold) { g_adminHoldDepth += hold ? 1 : -1; }
static bool nagCfgSaveCandidateWithScroll(const NagConfig &, const NagScrollStoredSnapshot &) { return g_saveOk; }
static void nagTsl9RuntimeReset() {}
static void nagCfgCommit(const NagConfig &c) { nagCfg = c; g_nagCommits++; }
static void tsl9InputHardResetAfterQuiesce(bool) {}
static bool g_isa = false, g_dms = false, g_vision = false, g_doorCancel = false;
static bool isaSuppressionControlSnapshot() { return g_isa; }
static bool isaSuppressionControlApply(bool v) { if (!g_saveOk) return false; g_isa = v; return true; }
static bool driverMonitoringControlSnapshot() { return g_dms; }
static bool driverMonitoringControlApply(bool v) { if (!g_saveOk) return false; g_dms = v; return true; }
static volatile bool blinkAEnabled = false, tlsscEnabled = false, preheatEnabled = false;
static bool setAutoBlinkerEnabled(bool v, bool) { if (!g_saveOk) return false; blinkAEnabled = v; return true; }
static bool setTlsscEnabled(bool v, bool) { if (!g_saveOk) return false; tlsscEnabled = v; return true; }
static bool preheatMenuSetEnabled(bool v) { if (!g_saveOk) return false; preheatEnabled = v; return true; }
struct FeatureCfgSnapshot { bool door = false; };
static FeatureCfgSnapshot featureCfgRuntimeSnapshot() { FeatureCfgSnapshot f; f.door = g_doorCancel; return f; }
static bool featureCfgPersistCandidate(const FeatureCfgSnapshot &) { return g_saveOk; }
static void featureCfgRuntimeRestore(const FeatureCfgSnapshot &f) { g_doorCancel = f.door; }
static bool visionControlSupported() { return g_supported; }
static bool visionControlRequestDisabled() { return g_vision; }
static uint8_t visionControlBusSnapshot() { return 0; }
static bool visionControlApplySettings(bool d, uint8_t) { if (!g_saveOk) return false; g_vision = d; return true; }
enum class ContApMethod : uint8_t { Unset, StalkDouble, ScrollSingle, ScrollDouble };
enum class ContApRoute : uint8_t { None, BodyA, ChassisB, PartyA };
struct ContApConfig { bool enabled = false; ContApMethod method = ContApMethod::Unset; };
static ContApConfig g_contAp;
static ContApConfig continuousApControlSnapshot() { return g_contAp; }
static ContApRoute continuousApGestureRoutePure(uint8_t, uint8_t, ContApMethod m) {
  return m == ContApMethod::Unset ? ContApRoute::None : ContApRoute::BodyA;
}
static bool continuousApControlApply(ContApConfig c) { if (!g_saveOk) return false; g_contAp = c; return true; }
// Pedal map (existing Pedal Response) and listen-only stand-ins.
#include "pedal_map_session_pure.h"
static bool g_pedalMapOk = true;
static std::vector<uint8_t> g_pedalRequests;
static inline bool activeProfilePedalMapSupported() { return g_supported; }
static bool requestPedalMapMode(uint8_t raw, const char *) {
  g_pedalRequests.push_back(raw);
  return g_pedalMapOk;
}
static bool g_passive = false;
static int g_listenRequests = 0;
static bool canUsbPassive() { return g_passive; }
static bool canUsbTxHeld() { return g_passive; }
static bool canUsbRequestMode(bool passive) { g_listenRequests++; g_passive = passive; return true; }

// ─── CAN TX stubs (record every frame) ───────────────────────────────────
struct can_frame { uint32_t can_id; uint8_t can_dlc; uint8_t data[8]; };
struct twai_message_t { uint32_t identifier; uint8_t data_length_code; uint8_t data[8]; };
namespace MCP2515 { enum ERROR { ERROR_OK = 0, ERROR_FAIL = 1 }; }
enum McpTxResultReason : uint8_t { MCP_TX_OK = 0 };
typedef int esp_err_t;
#define ESP_OK 0
static constexpr uint8_t CAN_TX_TRACE_SOURCE_BODY_COMFORT = 10;
struct TxRecord { uint8_t bus; uint16_t id; uint8_t data[8]; };
static std::vector<TxRecord> g_tx;
static uint32_t canTxEpochSnapshot() { return 1; }
static bool canTxMcpSendTagged(const struct can_frame *f, uint32_t, uint8_t source,
                               MCP2515::ERROR &err, McpTxResultReason *) {
  assert(source == CAN_TX_TRACE_SOURCE_BODY_COMFORT);
  TxRecord r = {1, (uint16_t)f->can_id, {}};
  memcpy(r.data, f->data, 8);
  g_tx.push_back(r);
  err = MCP2515::ERROR_OK;
  return true;
}
static esp_err_t canTxTwaiTransmitTagged(const twai_message_t *m, uint32_t, uint8_t) {
  TxRecord r = {2, (uint16_t)m->identifier, {}};
  memcpy(r.data, m->data, 8);
  g_tx.push_back(r);
  return ESP_OK;
}

#include "json_writer_arduino.h"
#include "body_comfort_runtime.h"

// ─── Simulated vehicle ───────────────────────────────────────────────────
struct Car {
  uint8_t gear = BC_GEAR_P;
  uint8_t pedalRaw = 0;     // 0.4 % steps
  uint8_t brake = 0;        // 0 OFF 1 ON
  bool handle[2] = {false, false};
  int32_t kph10 = 0;
  uint8_t latch[4] = {2, 2, 2, 2};      // CLOSED
  uint8_t mirror[2] = {2, 2};           // UNFOLDED
  uint8_t winPosState[4] = {1, 1, 1, 1};// CLOSED
  int16_t winPos[4] = {-200, -200, -200, -200};
  uint8_t winMotion[4] = {0, 0, 0, 0};
  bool rainy = false, snowy = false;
  uint8_t hazard = 0;
  bool indicator = false;
  bool camera = false;
  uint8_t rearUnbuckled[3] = {0, 0, 0};
  bool buttons[BC_BTN_COUNT] = {};
};
static Car car;
static uint8_t g_bus = BC_BUS_A;

static void rx(uint16_t id, const uint8_t d[8]) { bodyComfortObserve(g_bus, id, d, 8); }

static void sendFrames() {
  uint8_t d[8];
  memset(d, 0, 8); bcSetBitsPure(d, 21, 3, car.gear); bcSetBitsPure(d, 32, 8, car.pedalRaw);
  bcSetBitsPure(d, 19, 2, car.brake); rx(0x118, d);
  memset(d, 0, 8); bcSetBitsPure(d, 43, 13, (uint32_t)((car.kph10 + 400) * 5 / 4)); rx(0x257, d);
  for (uint8_t side = 0; side < 2; ++side) {
    const uint8_t front = side == 0 ? BC_DOOR_FL : BC_DOOR_FR;
    const uint8_t rear = side == 0 ? BC_DOOR_RL : BC_DOOR_RR;
    memset(d, 0, 8);
    bcSetBitsPure(d, 14, 4, car.latch[front]);
    bcSetBitsPure(d, 18, 4, car.latch[rear]);
    bcSetBitsPure(d, 52, 3, car.mirror[side]);
    bcSetBitsPure(d, 10, 1, car.handle[side] ? 1u : 0u);
    rx(side == 0 ? 0x102 : 0x103, d);
    memset(d, 0, 8);
    bcSetBitsPure(d, 1, 4, car.winMotion[front]);
    bcSetBitsPure(d, 22, 9, (uint32_t)car.winPos[front] & 0x1FFu);
    bcSetBitsPure(d, 33, 4, car.winMotion[rear]);
    bcSetBitsPure(d, 54, 9, (uint32_t)car.winPos[rear] & 0x1FFu);
    rx(side == 0 ? 0x2C2 : 0x2C3, d);
    memset(d, 0, 8);
    bcSetBitsPure(d, 0, 1, 1u);
    bcSetBitsPure(d, 49, 3, car.winPosState[front]);
    bcSetBitsPure(d, 52, 3, car.winPosState[rear]);
    rx(side == 0 ? 0x2C2 : 0x2C3, d);
  }
  memset(d, 0, 8); d[0] = (uint8_t)((car.rainy ? 1 : 0) | (car.snowy ? 2 : 0)); rx(0x25C, d);
  memset(d, 0, 8); bcSetBitsPure(d, 41, 1, car.camera ? 1 : 0); rx(0x353, d);
  memset(d, 0, 8); bcSetBitsPure(d, 4, 4, car.hazard); bcSetBitsPure(d, 0, 2, car.indicator ? 1 : 0); rx(0x3F5, d);
  // 0x3C2 mux0 and mux1 (all buttons OFF=1 unless pressed).
  memset(d, 0, 8); rx(0x3C2, d);
  memset(d, 0, 8); bcSetBitsPure(d, 0, 1, 1u);
  for (uint8_t b = 0; b < BC_BTN_COUNT; ++b)
    bcSetBitsPure(d, bcButtonStartBitPure(b), 2, car.buttons[b] ? BC_SWC_ON : 1u);
  rx(0x3C2, d);
  memset(d, 0, 8); d[2] = 0x11; rx(0x273, d);
  memset(d, 0, 8); rx(0x3B3, d);
  memset(d, 0, 8); d[0] = 0x40; rx(0x294, d);
  memset(d, 0, 8);
  for (uint8_t s = 0; s < 3; ++s) bcSetBitsPure(d, (uint8_t)(36 + s * 2), 2, car.rearUnbuckled[s]);
  d[7] = teslaAdditiveChecksumPure(0x3A1, d);
  rx(0x3A1, d);
}

// Advance simulated time in 10 ms service ticks with 100 ms CAN frames.
static void run(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 10) {
    g_now += 10;
    if (g_now % 100 == 0) sendFrames();
    bodyComfortServiceTick(g_now);
  }
}

static std::vector<TxRecord> takeTx(uint16_t id) {
  std::vector<TxRecord> out;
  for (const auto &r : g_tx) if (r.id == id) out.push_back(r);
  return out;
}

static void setCfg(const BodyComfortConfigPure &c) {
  bcCfg = c;
}

static void resetAll() {
  bcState = {};
  bcCfg = bodyComfortDefaultsPure();
  car = Car();
  g_tx.clear();
  g_nvs.clear();
  g_pedalRequests.clear();
  g_pedalMapOk = true;
  g_passive = false;
}

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
  run(400);
  auto w = takeTx(0x294);
  assert(w.size() == BC_ONE_SHOT_FRAMES);
  for (const auto &r : w) {
    assert(bcGetBitsPure(r.data, 52, 3) == BC_WINREQ_OPEN);
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
  run(800);
  w = takeTx(0x294);
  assert(w.size() == BC_ONE_SHOT_FRAMES);
  assert(bcGetBitsPure(w[0].data, 52, 3) == BC_WINREQ_CLOSED);
  assert(bcGetBitsPure(w[0].data, 48, 4) == 0x1u);
  assert(bcState.entry[BC_DOOR_FL].phase == BC_ENTRY_IDLE);
  assert(bcState.entryOpens == 1 && bcState.entryRestores == 1);

  // Rear-left partially open (50 %): restore uses GOTO_PERCENT 50.
  g_tx.clear();
  car.latch[BC_DOOR_RL] = 1;
  run(400);
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
  car.latch[BC_DOOR_FL] = 1;
  run(400);
  auto m = takeTx(0x273);
  assert(m.size() == BC_ONE_SHOT_FRAMES);
  assert(bcGetBitsPure(m[0].data, 24, 2) == BC_MIRROR_REQ_RETRACT && m[0].data[2] == 0x11);
  assert(bcState.mirrorFoldedByUs && bcState.mirrorFolds == 1);
  car.mirror[0] = car.mirror[1] = BC_MIRROR_FOLDED;
  car.latch[BC_DOOR_FL] = 2;
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
  // Short pull (normal door opening): nothing.
  car.handle[0] = true; run(800); car.handle[0] = false; run(500);
  assert(takeTx(0x273).empty());
  // Passenger not selected.
  car.handle[1] = true; run(3000); car.handle[1] = false; run(300);
  assert(takeTx(0x273).empty());
  // Driver hold >= 2 s opens the frunk once.
  car.handle[0] = true; run(1800);
  assert(takeTx(0x273).empty());
  run(600);
  auto f = takeTx(0x273);
  assert(!f.empty() && bcGetBitsPure(f[0].data, 5, 1) == 1u && f[0].data[2] == 0x11);
  assert(bcState.lastMacroButton == BC_HANDLE_BUTTON_BASE && bcState.lastMacroAction == BC_ACTION_OPEN_FRUNK);
  g_tx.clear();
  run(3000);  // still held: no repeat
  assert(takeTx(0x273).empty());
  car.handle[0] = false; run(300);
  assert(bcState.frunkOpens == 1);
  // Both doors selected: passenger works too.
  bcCfg.dhDoorMask = 0x03u;
  car.handle[1] = true; run(2500); car.handle[1] = false; run(300);
  assert(!takeTx(0x273).empty());
  // Not in P: refused, nothing sent.
  g_tx.clear();
  car.gear = BC_GEAR_D;
  car.handle[0] = true; run(2500); car.handle[0] = false; run(300);
  assert(takeTx(0x273).empty() && std::string(bcState.lastMacroResult) == "frunk only opens in P");
  car.gear = BC_GEAR_P;
  // Door-handle action can be any macro action, e.g. hazard blink.
  bcCfg.dhAction = BC_ACTION_TOGGLE_REVERSE_HAZARD;
  car.handle[0] = true; run(2500); car.handle[0] = false; run(300);
  assert(bcCfg.rvHazard);
  // Wheel macro can open the frunk as well; HTTP rejects the retired action.
  server.args = {{"dhAction", "4"}};
  httpBodyComfortUpdate();
  assert(server.lastCode == 400);
  server.args = {{"id", "20"}};
  httpBodyComfortAction();
  assert(server.lastCode == 200);
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

int main() {
  bodyComfortStart();
  assert(bodyComfortTaskHandle != nullptr);
  testUnsupportedTopologies();
  testAllOffByDefault();
  testEasyEntryOpenAndRestore();
  testEasyEntryBlocked();
  testMirrorFoldCameraUnfold();
  testSeatbelt();
  testReverseHazardAndLight();
  testDoorHazard();
  testHazardBlinkAndMacros();
  testHttp();
  testExistingFeatureMacros();
  testKickdown();
  testDoorHandleFrunk();
  testListenOnly();
  std::puts("body_comfort_runtime host: all tests passed");
  return 0;
}
