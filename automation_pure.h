#pragma once
#include <stdint.h>
#include <string.h>

static constexpr uint8_t AUTOMATION_RULES=16, AUTOMATION_CAMERA=255;
using AutomationMask=uint16_t;
static constexpr AutomationMask AUTOMATION_ALL=0xffff;
static constexpr unsigned AUTOMATION_RECORD_BYTES=208;
// Speeds are km/h; wire decoding retains hundredths to avoid rounding edges.
struct AutomationRule {
  bool enabled=false;
  uint8_t sensor=1, low=1, high=20, gear=4, action=AUTOMATION_CAMERA, trigger=0;
  uint16_t enterMs=0, exitMs=3000;
};
struct AutomationConfig { AutomationRule rules[AUTOMATION_RULES]; };
static inline bool automationRuleEqual(const AutomationRule&a,const AutomationRule&b){
  return a.enabled==b.enabled&&a.sensor==b.sensor&&a.low==b.low&&a.high==b.high&&a.gear==b.gear&&a.action==b.action&&a.trigger==b.trigger&&a.enterMs==b.enterMs&&a.exitMs==b.exitMs;
}
static inline bool automationConfigValid(const AutomationConfig& c,uint8_t lastAction) {
  unsigned cameras=0;
  for(const auto& r:c.rules){
    if(r.sensor<1||r.sensor>3||r.low>r.high||r.gear>4||r.trigger>1||r.enterMs>30000||r.exitMs>30000)return false;
    if(r.sensor==1 ? r.high>150 : (r.low<1||r.high>(r.sensor==2?4:2)))return false;
    if(r.action!=AUTOMATION_CAMERA&&(r.action==0||r.action>lastAction))return false;
    if(r.action==AUTOMATION_CAMERA){
      if(r.sensor!=1||r.gear!=4||r.trigger!=0)return false;
      if(r.enabled)++cameras;
    }
  }
  return cameras<=1;
}
struct AutomationSample {
  int32_t value=0;uint32_t at=0;bool valid=false;
  void set(int32_t v,uint32_t now,bool ok=true){value=v;at=now;valid=ok;}
  bool fresh(uint32_t now,uint32_t age=500)const{return valid&&uint32_t(now-at)<=age;}
};
struct AutomationSensors {
  AutomationSample speed,gear,brake,camera,fascia;
  void observe(uint16_t id,const uint8_t* d,uint8_t dlc,uint32_t now){
    if(!d||dlc!=8)return;
    if(id==0x257){const uint16_t raw=(uint16_t(d[5]>>3)|(uint16_t(d[6])<<5))&8191;speed.set(int32_t(raw)*8-4000,now,raw!=8191);}
    if(id==0x118){const uint8_t g=(d[2]>>5)&7;gear.set(g,now,g>=1&&g<=4);}
    if(id==0x39d){const uint8_t b=d[2]&3;brake.set(b,now,b==1||b==2);}
    if(id==0x353)camera.set((d[5]>>1)&1,now);
    if(id==0x294)fascia.set((d[4]>>7)&1,now);
  }
  bool cameraFresh(uint32_t now)const{return camera.fresh(now,1000)&&fascia.fresh(now,1000);}
  bool cameraOn()const{return camera.value||fascia.value;}
};
struct AutomationEdges {AutomationMask entered=0,left=0,invalid=0,active=0;};
class AutomationEngine {
 public:
  void reset(){*this=AutomationEngine();}
  void resetRule(unsigned i){if(i<AUTOMATION_RULES)states_[i]=State{};}
  AutomationEdges evaluate(const AutomationConfig& c,const AutomationSensors& s,uint32_t now){
    AutomationEdges out;
    for(unsigned i=0;i<AUTOMATION_RULES;++i){
      const auto& r=c.rules[i];auto& st=states_[i];const AutomationMask bit=AutomationMask(1u<<i);
      const auto& v=r.sensor==1?s.speed:(r.sensor==2?s.gear:s.brake);
      if(!r.enabled||!v.fresh(now)||(r.gear&&!s.gear.fresh(now))){st=State{};out.invalid|=bit;continue;}
      const int32_t scale=r.sensor==1?100:1;
      const bool match=v.value>=int32_t(r.low)*scale&&v.value<=int32_t(r.high)*scale&&(!r.gear||s.gear.value==r.gear);
      // Establish a baseline after boot, configuration changes or stale RX.
      // Resumption is not an edge and must not replay an old action.
      if(!st.seen){st.seen=true;st.active=match;st.pending=match;st.since=now;}
      else if(match==st.active){st.pending=match;st.since=now;}
      else {
        if(st.pending!=match){st.pending=match;st.since=now;}
        if(uint32_t(now-st.since)>=(match?r.enterMs:r.exitMs)){
          st.active=match;if(match)out.entered|=bit;else out.left|=bit;
        }
      }
      if(st.active)out.active|=bit;
    }
    return out;
  }
 private:
  struct State{bool seen=false,active=false,pending=false;uint32_t since=0;};
  State states_[AUTOMATION_RULES];
};

// Camera is a toggle request, not an absolute ON/OFF command. Ownership needs
// an observed OFF -> ON after our accepted press, and is never persisted.
class AutomationCamera {
 public:
  void reset(){*this=AutomationCamera();}
  bool owned()const{return owned_;}
  bool busy()const{return target_||waiting_;}
  bool startingOpen()const{return target_==1&&!pressSent_;}
  bool startingClose()const{return target_==2&&!pressSent_;}
  void cancelUnsentClose(){if(startingClose()){target_=0;waiting_=false;}}
  uint8_t status()const{return owned_?2:(busy()?1:0);}
  void observe(uint16_t id,const uint8_t*d,uint8_t dlc,uint32_t now){
    if(!d||dlc!=8)return;
    if(id==0x3c2&&(d[0]&1)){
      if(!(d[6]&0x10)||((d[6]>>2)&3)){reset();return;}
      memcpy(stock_,d,8);stockAt_=now;stockSeen_=true;
    }
  }
  bool freshStock(uint32_t now)const{return stockSeen_&&uint32_t(now-stockAt_)<=150;}
  void maintain(const AutomationSensors& s,uint32_t now,bool allowed){
    if(!allowed||!s.gear.fresh(now)||s.gear.value!=4||!s.speed.fresh(now)||!s.cameraFresh(now)||!freshStock(now)){reset();return;}
    // A screen/manual activation racing the first TX must not be toggled OFF.
    if(startingOpen()&&s.cameraOn()){reset();return;}
    if((target_||waiting_)&&uint32_t(now-requestAt_)>=1500){reset();return;}
    if(pressSent_&&opening_&&s.cameraOn()&&
       ((s.camera.value&&int32_t(s.camera.at-pressAt_)>0)||(s.fascia.value&&int32_t(s.fascia.at-pressAt_)>0))){owned_=true;waiting_=false;}
    if(owned_&&!s.cameraOn()){reset();return;}
  }
  bool open(const AutomationSensors& s,uint32_t now){
    if(busy()||owned_||!s.cameraFresh(now)||s.cameraOn()||!freshStock(now))return false;
    start(true,now);return true;
  }
  bool close(const AutomationSensors& s,uint32_t now){
    if(!owned_||busy()||!s.cameraFresh(now)||!s.cameraOn()||!freshStock(now))return false;
    start(false,now);return true;
  }
  // Compose from the newest physical stock frame under the final TX barrier.
  // Press is bounded to 100 ms; a natural stock release is never suppressed.
  uint8_t prepare(uint8_t*d,uint32_t now)const{
    if(!target_||!freshStock(now))return 0;
    memcpy(d,stock_,8);
    const bool release=pressSent_&&uint32_t(now-pressAt_)>=100;
    d[6]=(d[6]&uint8_t(~0x0c))|(release?0:0x04);
    return release?2:1;
  }
  void acknowledge(uint8_t kind,bool ok,uint32_t now){
    if(!kind||!target_)return;
    if(!ok){reset();return;}
    if(kind==1&&!pressSent_){pressSent_=true;pressAt_=now;}
    if(kind==2){target_=0;waiting_=opening_&&!owned_;if(!opening_)reset();}
  }
 private:
  void start(bool opening,uint32_t now){opening_=opening;target_=opening?1:2;pressSent_=false;waiting_=false;requestAt_=now;}
  uint8_t stock_[8]={},target_=0;uint32_t stockAt_=0,requestAt_=0,pressAt_=0;
  bool stockSeen_=false,owned_=false,opening_=false,pressSent_=false,waiting_=false;
};
