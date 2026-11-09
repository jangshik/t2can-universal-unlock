#pragma once
#include "wifi_ap_visibility_pure.h"

// Owned by the existing core-0 web task. No new task, CAN lock, ISR, or timer.
static WifiApVisibilityConfig wifiApVisibilityConfig;
static bool wifiApVisibilityNvsError=false,wifiApVisibilityKnown=false,wifiApSsidHidden=false;
static bool wifiApVisibilityFallback=false,wifiApVisibilityLastTarget=false;
static uint8_t wifiApVisibilityAttempts=0;
static uint64_t wifiApVisibilityNotBeforeMs=0,wifiApVisibilityNextTryMs=0;
static esp_err_t wifiApVisibilityError=ESP_OK;
static uint64_t wifiApVisibilityNowMs(){return uint64_t(esp_timer_get_time())/1000;}
static bool wifiApVisibilityTarget(uint64_t now) {
  return !wifiApVisibilityFallback&&wifiApVisibilityHiddenAt(wifiApVisibilityConfig,now);
}
static void wifiApVisibilityArm(uint64_t notBefore) {
  wifiApVisibilityNotBeforeMs=notBefore;wifiApVisibilityNextTryMs=0;
  wifiApVisibilityAttempts=0;wifiApVisibilityError=ESP_OK;
  wifiApVisibilityLastTarget=wifiApVisibilityTarget(wifiApVisibilityNowMs());
}
static void wifiApVisibilityLoad() {
  uint8_t bytes[8]={};bool found=false;WifiApVisibilityConfig c;
  wifiApVisibilityNvsError=!nvsCheckedReadBlob("wifiap","visibility",bytes,sizeof(bytes),found);
  if(!wifiApVisibilityNvsError&&found)wifiApVisibilityNvsError=!wifiApVisibilityDecode(bytes,c);
  wifiApVisibilityConfig=wifiApVisibilityNvsError?WifiApVisibilityConfig{}:c;
  wifiApVisibilityKnown=false;wifiApVisibilityFallback=false;wifiApVisibilityArm(0);
}
static void wifiApVisibilityApStarted(bool hidden) {
  wifiApVisibilityKnown=true;wifiApSsidHidden=hidden;wifiApVisibilityArm(0);
}
static bool wifiApVisibilitySave(const WifiApVisibilityConfig& c) {
  if(!wifiApVisibilityValid(c))return false;
  if(wifiApVisibilityNvsError||c.mode!=wifiApVisibilityConfig.mode||c.hideAfterSec!=wifiApVisibilityConfig.hideAfterSec){
    uint8_t bytes[8];wifiApVisibilityEncode(c,bytes);
    if(!nvsCheckedWriteBlob("wifiap","visibility",bytes,sizeof(bytes))){wifiApVisibilityNvsError=true;return false;}
  }
  wifiApVisibilityConfig=c;wifiApVisibilityNvsError=false;wifiApVisibilityFallback=false;
  // Send the HTTP response before changing discoverability, without sleeping.
  wifiApVisibilityArm(wifiApVisibilityNowMs()+600);return true;
}
static esp_err_t wifiApVisibilitySetLive(bool hidden) {
  wifi_config_t config={};
  esp_err_t error=esp_wifi_get_config(WIFI_IF_AP,&config);
  if(error!=ESP_OK){wifiApVisibilityKnown=false;return error;}
  wifiApVisibilityKnown=true;wifiApSsidHidden=config.ap.ssid_hidden!=0;
  if(wifiApSsidHidden==hidden)return ESP_OK;
  // NVS is written only on explicit settings saves, never at the timer deadline.
  error=esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if(error!=ESP_OK)return error;
  config.ap.ssid_hidden=hidden?1:0;
  error=esp_wifi_set_config(WIFI_IF_AP,&config);
  if(error!=ESP_OK)return error;
  error=esp_wifi_get_config(WIFI_IF_AP,&config);
  if(error!=ESP_OK){wifiApVisibilityKnown=false;return error;}
  wifiApSsidHidden=config.ap.ssid_hidden!=0;
  return wifiApSsidHidden==hidden?ESP_OK:ESP_FAIL;
}
static void wifiApVisibilityService() {
  const uint64_t now=wifiApVisibilityNowMs();const bool target=wifiApVisibilityTarget(now);
  if(target!=wifiApVisibilityLastTarget){
    wifiApVisibilityLastTarget=target;wifiApVisibilityAttempts=0;wifiApVisibilityNextTryMs=0;wifiApVisibilityError=ESP_OK;
  }
  if(wifiApVisibilityKnown&&wifiApSsidHidden==target)return;
  if(now<wifiApVisibilityNotBeforeMs||now<wifiApVisibilityNextTryMs||wifiApVisibilityAttempts>=3)return;
  ++wifiApVisibilityAttempts;wifiApVisibilityError=wifiApVisibilitySetLive(target);
  wifiApVisibilityNextTryMs=now+5000;
}
