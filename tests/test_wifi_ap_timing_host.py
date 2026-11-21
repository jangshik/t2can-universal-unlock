"""Host regression for Wi-Fi AP slow start and hidden SSID (web_api.h).

Extracts the production Wi-Fi access-point section of web_api.h, compiles it
against small WiFi/Preferences/WebServer stubs and checks:
  * timing settings persist in the preserved "wifiap" namespace,
  * POST /api/wifi/timing validates input and rejects partial garbage,
  * the hidden SSID is applied only after the configured delay, never in
    profile setup mode, survives an SSID apply and is reverted when disabled,
  * webTask honours the start delay before starting the AP.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WEB = (ROOT / "web_api.h").read_text(encoding="utf-8")

start = WEB.index("// ─── Wi-Fi access-point configuration")
end = WEB.index("// ─── S3XY BLE multi-device HTTP")
section = WEB[start:end]

# webTask contract: the slow-start delay is applied before the first AP start
# and the hide service runs from the web loop.
task = WEB[WEB.index("static void webTask(void *arg) {"):]
assert task.index("wifiApTimingLoad();") < task.index("wifiApStartDelayMsPure(") < task.index("wifiApActivate(ssid, password, true)")
assert "vTaskDelay(pdMS_TO_TICKS(delayMs));" in task
assert "wifiApTimingService();" in task
assert 'server.on("/api/wifi/timing", HTTP_GET, httpWifiTimingStatus);' in WEB
assert 'server.on("/api/wifi/timing", HTTP_POST, httpWifiTimingApply);' in WEB
# Registered with the always-available routes (before the profile gate).
assert WEB.index('server.on("/api/wifi/timing", HTTP_POST') < WEB.index("if (!vehicleProfileSetupMode && !vehicleProfileNvsError) {\n    server.on(\"/api/snapshot\"")
# Wi-Fi settings stay in the preserved namespace; Body Comfort is reset.
assert '"preheat", "bodycmf"' in WEB

HARNESS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

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
  const char *c_str() const { return v.c_str(); }
  bool operator==(const char *s) const { return v == s; }
  bool operator==(const String &s) const { return v == s.v; }
  bool operator!=(const String &s) const { return v != s.v; }
  char operator[](size_t i) const { return v[i]; }
  String &operator+=(const char *s) { v += (s ? s : ""); return *this; }
  String &operator+=(char c) { v += c; return *this; }
  String &operator+=(unsigned long x) { v += std::to_string(x); return *this; }
  String &operator+=(long x) { v += std::to_string(x); return *this; }
  String &operator+=(unsigned int x) { v += std::to_string(x); return *this; }
  String &operator+=(int x) { v += std::to_string(x); return *this; }
  String &operator+=(const String &s) { v += s.v; return *this; }
};
static String operator+(const String &a, const char *b) { String r = a; r += b; return r; }
static String operator+(const char *a, const String &b) { String r(a); r += b; return r; }
static String operator+(const String &a, const String &b) { String r = a; r += b; return r; }

static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static void delay(uint32_t ms) { g_now += ms; }
#define pdMS_TO_TICKS(x) (x)
static void vTaskDelay(uint32_t ms) { g_now += ms; }
#define T2CAN_SERIAL_PRINTF(...) do { if (0) std::printf(__VA_ARGS__); } while (0)
#define T2CAN_SERIAL_PRINTLN(...) do { if (0) std::puts(__VA_ARGS__); } while (0)
static volatile bool vehicleProfileSetupMode = false;
static volatile bool vehicleProfileNvsError = false;

enum class JsonTextPolicy { Wifi, Research, Ble };
static String escapedTextArduino(const char *s, size_t, JsonTextPolicy) { return String(s); }

struct IPAddress { String toString() const { return String("192.168.4.1"); } };
#define WIFI_AP 2
struct WiFiStub {
  int softApCalls = 0;
  std::string ssid, pass;
  int hidden = -1;
  bool fail = false;
  bool softAP(const char *s, const char *p, int channel = 1, int ssidHidden = 0) {
    assert(channel == 1);
    softApCalls++;
    if (fail) return false;
    ssid = s; pass = p; hidden = ssidHidden;
    return true;
  }
  void softAPdisconnect(bool) {}
  void mode(int) {}
  void softAPmacAddress(uint8_t *mac) { memset(mac, 0xAB, 6); }
  IPAddress softAPIP() { return IPAddress(); }
};
static WiFiStub WiFi;

static std::map<std::string, std::string> g_nvs;
class Preferences {
 public:
  bool begin(const char *ns, bool) { ns_ = ns; return true; }
  void end() {}
  bool getBool(const char *k, bool d) { auto it = g_nvs.find(ns_ + "/" + k); return it == g_nvs.end() ? d : it->second == "1"; }
  uint16_t getUShort(const char *k, uint16_t d) { auto it = g_nvs.find(ns_ + "/" + k); return it == g_nvs.end() ? d : (uint16_t)std::stoul(it->second); }
  String getString(const char *k, const char *d) { auto it = g_nvs.find(ns_ + "/" + k); return String(it == g_nvs.end() ? d : it->second.c_str()); }
  size_t putBool(const char *k, bool v) { g_nvs[ns_ + "/" + k] = v ? "1" : "0"; return 1; }
  size_t putUShort(const char *k, uint16_t v) { g_nvs[ns_ + "/" + k] = std::to_string(v); return 2; }
  size_t putString(const char *k, const String &v) { g_nvs[ns_ + "/" + k] = v.v; return v.v.size() + 1; }
 private:
  std::string ns_;
};

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

#include "body_comfort_pure.h"
#include "json_writer_arduino.h"

SECTION

int main() {
  // Defaults: no delay, not hidden.
  wifiApTimingLoad();
  assert(!wifiApTiming.slowStartEnabled && !wifiApTiming.hideEnabled);
  assert(wifiApStartDelayMsPure(wifiApTiming, false) == 0u);

  // Validation.
  server.args = {{"startDelayS", "601"}};
  httpWifiTimingApply();
  assert(server.lastCode == 400);
  server.args = {{"slowStartEnabled", "1"}, {"hideDelayS", "abc"}};
  httpWifiTimingApply();
  assert(server.lastCode == 400 && !wifiApTiming.slowStartEnabled);
  server.args = {};
  httpWifiTimingApply();
  assert(server.lastCode == 400);

  // Save slow start + hide and reload from NVS.
  server.args = {{"slowStartEnabled", "1"}, {"startDelayS", "45"}, {"hideEnabled", "true"}, {"hideDelayS", "20"}};
  httpWifiTimingApply();
  assert(server.lastCode == 200);
  assert(server.lastBody.find("\"startDelayS\":45") != std::string::npos);
  wifiApTiming = wifiApTimingDefaultsPure();
  wifiApTimingLoad();
  assert(wifiApTiming.slowStartEnabled && wifiApTiming.startDelayS == 45);
  assert(wifiApTiming.hideEnabled && wifiApTiming.hideDelayS == 20);
  assert(wifiApStartDelayMsPure(wifiApTiming, false) == 45000u);
  assert(wifiApStartDelayMsPure(wifiApTiming, true) == 0u);

  // AP starts visible.
  assert(wifiApActivate("T2CAN-TEST", "password123", true));
  assert(WiFi.hidden == 0);
  wifiApStartedMs = millis();
  wifiApStarted = true;
  g_now += 19000;
  wifiApTimingService();
  assert(!wifiApHiddenActive && WiFi.hidden == 0);
  g_now += 1000;
  wifiApTimingService();
  assert(wifiApHiddenActive && WiFi.hidden == 1 && WiFi.ssid == "T2CAN-TEST");

  // Hidden state survives an SSID/password apply.
  server.args = {{"ssid", "T2CAN-NEW"}, {"password", "password456"}};
  httpWifiApply();
  assert(server.lastCode == 200 && WiFi.ssid == "T2CAN-NEW" && WiFi.hidden == 1);

  // Disabling hide makes the SSID visible again.
  server.args = {{"hideEnabled", "0"}};
  httpWifiTimingApply();
  assert(server.lastCode == 200);
  wifiApTimingService();
  assert(!wifiApHiddenActive && WiFi.hidden == 0);

  // Setup mode never hides.
  server.args = {{"hideEnabled", "1"}, {"hideDelayS", "0"}};
  httpWifiTimingApply();
  vehicleProfileSetupMode = true;
  wifiApTimingService();
  assert(!wifiApHiddenActive);
  vehicleProfileSetupMode = false;
  wifiApTimingService();
  assert(wifiApHiddenActive);

  // A failed re-announce retries later instead of spinning.
  server.args = {{"hideEnabled", "0"}};
  httpWifiTimingApply();
  wifiApTimingService();
  assert(!wifiApHiddenActive);
  server.args = {{"hideEnabled", "1"}, {"hideDelayS", "5"}};
  httpWifiTimingApply();
  WiFi.fail = true;
  wifiApTimingService();
  assert(!wifiApHiddenActive);
  WiFi.fail = false;
  wifiApTimingService();
  assert(!wifiApHiddenActive);  // waits a new delay window
  g_now += 5000;
  wifiApTimingService();
  assert(wifiApHiddenActive);

  httpWifiTimingStatus();
  assert(server.lastBody.find("\"hiddenNow\":true") != std::string::npos);
  std::puts("wifi ap timing host: all tests passed");
  return 0;
}
'''

with tempfile.TemporaryDirectory() as td:
    src = Path(td) / "wifi_timing.cpp"
    exe = Path(td) / "wifi_timing"
    src.write_text(HARNESS.replace("SECTION", section), encoding="utf-8")
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-function", "-I", str(ROOT), str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("test_wifi_ap_timing_host: OK")
