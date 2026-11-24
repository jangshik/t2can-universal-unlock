// Shared host harness for body_comfort_runtime.h + automation_runtime.h:
// minimal Arduino / FreeRTOS / CAN stubs and a simulated Juniper Body CAN.
// Included by tests/test_body_comfort_runtime_host.cpp and
// tests/test_automation_runtime_host.cpp.
#pragma once
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
#include "automation_runtime.h"
// Not every test file calls every handler.
[[maybe_unused]] static void (*const kAutoHandlers[])() = {
    automationLoad, httpAutomationGet, httpAutomationCatalog, httpAutomationRule,
    httpAutomationDelete, httpAutomationGlobal, httpAutomationFlag, httpAutomationRun};
[[maybe_unused]] static void (*const kBodyHandlers[])() = {
    bodyComfortCfgLoad, bodyComfortStart, httpBodyComfortConfig, httpBodyComfortUpdate,
    httpBodyComfortWindowTest, httpBodyComfortAction};

// ─── Simulated vehicle ───────────────────────────────────────────────────
struct Car {
  uint8_t gear = BC_GEAR_P;
  uint8_t pedalRaw = 0;     // 0.4 % steps
  uint8_t brake = 0;        // 0 OFF 1 ON
  bool handle[2] = {false, false};
  bool honk = false;
  int vc3Mode = 0;                 // 0 periodic, 1 only while vc3Once, 2 never
  bool vc3Once = false;
  uint8_t lockSimple = 1;          // 1 UNLOCKED, 2 LOCKED, 0 = frame absent
  bool frontLocked[2] = {false, false};
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
  if (car.lockSimple) {
    memset(d, 0, 8);
    bcSetBitsPure(d, 54, 2, car.lockSimple);
    bcSetBitsPure(d, 19, 1, car.frontLocked[0] ? 1u : 0u);
    bcSetBitsPure(d, 21, 1, car.frontLocked[1] ? 1u : 0u);
    rx(0x339, d);
  }
  memset(d, 0, 8); bcSetBitsPure(d, 41, 1, car.camera ? 1 : 0); rx(0x353, d);
  memset(d, 0, 8); bcSetBitsPure(d, 4, 4, car.hazard); bcSetBitsPure(d, 0, 2, car.indicator ? 1 : 0); rx(0x3F5, d);
  // 0x3C2 mux0 and mux1 (all buttons OFF=1 unless pressed).
  memset(d, 0, 8); rx(0x3C2, d);
  memset(d, 0, 8); bcSetBitsPure(d, 0, 1, 1u);
  for (uint8_t b = 0; b < BC_BTN_COUNT; ++b)
    bcSetBitsPure(d, bcButtonStartBitPure(b), 2, car.buttons[b] ? BC_SWC_ON : 1u);
  rx(0x3C2, d);
  memset(d, 0, 8); d[2] = 0x11; bcSetBitsPure(d, 61, 1, car.honk ? 1u : 0u); rx(0x273, d);
  memset(d, 0, 8); rx(0x3B3, d);
  if (car.vc3Mode == 0 || (car.vc3Mode == 1 && car.vc3Once)) {
    memset(d, 0, 8); d[0] = 0x40; d[2] = 0xC0; bcSetBitsPure(d, 17, 1, car.vc3Once ? 1u : 0u); rx(0x294, d);
    car.vc3Once = false;
  }
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

[[maybe_unused]] static void setCfg(const BodyComfortConfigPure &c) {
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
  for (auto &r : autoRules) r = {};
  for (auto &r : autoRt) r = {};
  autoEnabled = false;
  autoFlags = 0;
  autoGlobalDirty = false;
  autoLogHead = autoLogCount = 0;
  autoRateWindowMs = 0;
  autoRateCount = 0;
  autoRateLimited = 0;
  autoTicked = false;
  autoActionsRun = 0;
  autoDriveBlocked = 0;
  autoSaveFailures = 0;
}

