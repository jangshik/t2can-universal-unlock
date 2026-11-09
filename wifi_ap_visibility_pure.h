#pragma once
#include <stdint.h>
#include <string.h>

// Visibility is independent of AP credentials. Zero seconds means hidden as
// soon as the AP starts. The deadline is measured from boot, not from saving.
struct WifiApVisibilityConfig {
  uint8_t mode=0; // 0 visible, 1 hidden, 2 hidden after boot delay
  uint16_t hideAfterSec=60;
};
static inline bool wifiApVisibilityValid(const WifiApVisibilityConfig& c) {
  return c.mode<=2&&c.hideAfterSec<=3600;
}
static inline bool wifiApVisibilityHiddenAt(const WifiApVisibilityConfig& c,uint64_t uptimeMs) {
  return c.mode==1||(c.mode==2&&uptimeMs>=uint64_t(c.hideAfterSec)*1000);
}
static inline void wifiApVisibilityEncode(const WifiApVisibilityConfig& c,uint8_t b[8]) {
  memset(b,0,8);b[0]=1;b[1]=c.mode;b[2]=c.hideAfterSec&255;b[3]=c.hideAfterSec>>8;
}
static inline bool wifiApVisibilityDecode(const uint8_t b[8],WifiApVisibilityConfig& c) {
  if(b[0]!=1||b[4]||b[5]||b[6]||b[7])return false;
  WifiApVisibilityConfig next;next.mode=b[1];next.hideAfterSec=uint16_t(b[2])|(uint16_t(b[3])<<8);
  if(!wifiApVisibilityValid(next))return false;
  c=next;return true;
}
static inline bool wifiApVisibilityDecimal(const char* raw,uint16_t maximum,uint16_t& value) {
  if(!raw||!*raw)return false;
  uint32_t result=0;unsigned length=0;
  for(;*raw;++raw){if(++length>4||*raw<'0'||*raw>'9')return false;result=result*10+(*raw-'0');}
  if(result>maximum)return false;
  value=result;return true;
}
