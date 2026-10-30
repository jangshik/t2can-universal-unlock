#pragma once
#include <stdint.h>
#include <string.h>
#include "tesla_can_fields_pure.h"

// Optional Body CAN features. This module consumes validated stock RX only;
// routing, profile support and final transport admission remain runtime duties.
struct ConvenienceConfig {
  bool kickdown=false, easyEntry=false, mirrorSync=false;
  bool rearBeltLeft=false, rearBeltCenter=false, rearBeltRight=false;
  bool reverseHazard=false, reverseFog=false;
  uint8_t kickOnPct=90, kickOffPct=10;
  // UI_windowPercentageRequest: signed six-bit raw, scale 5.
  // Automatic transmission is controlled by the default-OFF easyEntry toggle.
  uint8_t windowDropRaw=20;
  // 0: pedal release, 1: brake release, 2: either. A brake always inhibits TX.
  uint8_t kickReleaseMode=2;
};
static inline bool convenienceConfigValidPure(const ConvenienceConfig& c) {
  return c.kickOnPct<=100 && c.kickOnPct>c.kickOffPct && c.kickReleaseMode<=2;
}
struct ConvenienceFrame {
  uint16_t id=0;
  uint8_t dlc=8, data[8]={};
  // Local completion metadata; never placed on CAN.
  uint8_t effect=0, mask=0;
};
struct ConvenienceOutput {
  uint8_t count=0;
  ConvenienceFrame frames[4];
};
class ConvenienceEngine {
 public:
  void reset(uint32_t epoch) { *this=ConvenienceEngine(); epoch_=epoch; }
  bool windowTestReady(uint32_t now) const {
    return fresh(gearSeen_,gearMs_,now,500)&&gear_==1&&
      fresh(windowTemplateSeen_,windowTemplateMs_,now,250)&&(windowTemplate_[6]&0x70)==0;
  }
  bool prepareWindowTest(uint8_t type,uint8_t percent,uint8_t corner,uint32_t now,ConvenienceFrame &frame) {
    if(!windowTestReady(now)||corner>3||type<1||type>4||percent>100||percent%5)return false;
    ConvenienceOutput out;windowCommand(out,type,type==1?percent/5:0,1u<<corner,0,now);
    if(out.count!=1)return false;
    frame=out.frames[0];return true;
  }
  uint8_t observedDoor(uint8_t corner) const {return door_[corner];}
  uint8_t observedWindow(uint8_t corner) const {return window_[corner];}
  uint32_t doorAge(uint8_t corner,uint32_t now) const {return doorSeen_[corner]?now-doorMs_[corner]:UINT32_MAX;}
  uint32_t windowAge(uint8_t corner,uint32_t now) const {return windowSeen_[corner]?now-windowMs_[corner]:UINT32_MAX;}
  bool kickdownActive() const { return kickApplied_; }
  bool hazardOwned() const { return hazardOwned_; }
  bool hazardThreeFlashReady(uint32_t now) const {
    return burst_==Burst::Idle&&!hazardOwned_&&!hazardRequest_&&!hazardPulse_&&
      (!hazardPulseSeen_||uint32_t(now-hazardMs_)>=1000)&&
      fresh(gearSeen_,gearMs_,now,500)&&gear_!=2&&
      fresh(indicatorSeen_,indicatorMs_,now,500)&&lightingIdle_&&
      fresh(hazardButtonSeen_,hazardButtonMs_,now,250)&&!hazardButtonPressed_;
  }
  bool requestHazardThreeFlash(uint32_t now) {
    if(!hazardThreeFlashReady(now))return false;
    burst_=Burst::Starting;burstMs_=now;burstCount_=0;burstHigh_=false;
    hazardRequest_=true;hazardRequestedTarget_=true;return true;
  }
  bool hazardThreeFlashActive() const {return burst_!=Burst::Idle;}
  uint8_t hazardThreeFlashCount() const {return burstCount_;}
  bool hazardPulseValid(bool target,uint32_t now) const {
    return hazardPulse_&&hazardTarget_==target&&!hazardButtonPressed_&&
      uint32_t(now-hazardMs_)<200&&fresh(gearSeen_,gearMs_,now,500)&&
      (burst_==Burst::Idle||fresh(indicatorSeen_,indicatorMs_,now,500));
  }
  void acknowledge(const ConvenienceFrame& f,bool sent) {
    if(!sent)return;
    if(f.effect==1) { for(uint8_t i=0;i<4;++i)if(f.mask&(1u<<i))dropped_[i]=true; }
    if(f.effect==2) { for(uint8_t i=0;i<4;++i)if(f.mask&(1u<<i))dropped_[i]=false; }
    if(f.effect==3&&hazardPulse_&&hazardTarget_){hazardOwned_=true;if(burst_==Burst::Starting)burst_=Burst::Counting;}
    if(f.effect==4&&hazardPulse_&&!hazardTarget_){hazardOwned_=false;if(burst_==Burst::Stopping)burst_=Burst::Idle;}
  }
  ConvenienceOutput process(const ConvenienceConfig& c,uint16_t id,
      const uint8_t* d,uint8_t dlc,uint32_t now,uint32_t epoch,bool txAllowed) {
    ConvenienceOutput out;
    if(epoch!=epoch_)reset(epoch);
    if(!d||dlc>8)return out;
    if(!convenienceConfigValidPure(c)||!txAllowed){reset(epoch);return out;}
    // Loss of current drive inputs clears ownership instead of replaying stale
    // commands when the bus returns. Unsigned subtraction handles millis wrap.
    if(!fresh(gearSeen_,gearMs_,now,500)){kickApplied_=false;hazardPulse_=false;hazardRequest_=false;hazardOwned_=false;burst_=Burst::Idle;gearSeen_=false;}
    if(!c.kickdown)kickApplied_=false;
    if(burst_!=Burst::Idle){
      // No stale replay or blind OFF toggle after loss of feedback.
      if(!fresh(indicatorSeen_,indicatorMs_,now,500))cancelHazardBurst();
      else if(uint32_t(now-burstMs_)>=6000){
        if(burst_==Burst::Counting&&hazardReason_==1)stopHazardBurst();
        else if(burst_!=Burst::Stopping)cancelHazardBurst();
      }
    }
    if(!c.reverseHazard&&burst_==Burst::Idle){
      if(hazardOwned_){hazardRequest_=true;hazardRequestedTarget_=false;}
      else if(hazardRequest_&&hazardRequestedTarget_)hazardRequest_=false;
    }
    if(!c.easyEntry)memset(dropped_,0,sizeof(dropped_));
    if(id==0x118 && dlc>=8){
      const uint8_t next=(d[2]>>5)&7;
      const bool valid=next>=1&&next<=4;
      const bool edge=gearSeen_&&valid&&gear_!=next;
      if(c.reverseHazard&&edge&&next==2&&burst_!=Burst::Idle){
        // Reverse takes over a successfully started burst; do not toggle OFF.
        if(hazardOwned_){burst_=Burst::Idle;hazardRequest_=false;if(!hazardTarget_)hazardPulse_=false;}
        else cancelHazardBurst();
      }
      if(c.reverseHazard && edge && (next==2 || gear_==2)){
        const bool hazards=indicatorLeft_&&indicatorRight_;
        const bool canStart=next==2&&!hazardOwned_&&fresh(indicatorSeen_,indicatorMs_,now,500)&&!hazards;
        const bool canStop=next!=2&&hazardOwned_;
        if(canStart||canStop){hazardRequest_=true;hazardRequestedTarget_=canStart;}
        else if(next!=2&&hazardRequest_&&hazardRequestedTarget_)hazardRequest_=false;
      }
      gear_=next;gearMs_=now;gearSeen_=valid;
      accel_=d[4];accelMs_=now;accelSeen_=accel_<=250;
      if(!valid||next!=4||!accelSeen_)kickApplied_=false;
      if(c.kickReleaseMode!=1&&uint16_t(accel_)*2<uint16_t(c.kickOffPct)*5)kickApplied_=false;
    }
    if(id==0x39D && dlc>=3){brake_=d[2]&3;brakeSeen_=brake_==1||brake_==2;brakeMs_=now;
      if(!brakeSeen_||(brake_==2&&c.kickReleaseMode!=0))kickApplied_=false;}
    if(id==0x61D&&dlc==8){wet_=(d[7]&3)!=0;weatherSeen_=true;weatherMs_=now;}
    if(id==0x3F5&&dlc==8){
      indicatorLeft_=(d[6]>>2)&3;indicatorRight_=(d[6]>>4)&3;
      indicatorSeen_=indicatorLeft_<3&&indicatorRight_<3;indicatorMs_=now;
      // DBC VAL_: request 0 OFF, 1 ACTIVE_LOW, 2 ACTIVE_HIGH. Internal
      // fields have no value table, so only request fields count flash edges.
      const uint8_t left=d[0]&3,right=(d[0]>>2)&3;
      hazardReason_=d[0]>>4;
      lightingIdle_=left==0&&right==0&&hazardReason_==0&&(d[6]&0x3e)==0;
      if(burst_!=Burst::Idle){
        if(!indicatorSeen_||left==3||right==3||(d[6]&2)||hazardReason_>1)cancelHazardBurst();
        else if(burst_==Burst::Starting&&!lightingIdle_)cancelHazardBurst();
        else if(burst_==Burst::Counting){
          if(hazardReason_==0){
            if(burstCount_||uint32_t(now-burstMs_)>=1000)cancelHazardBurst();
          }else{
            const bool high=left==2&&right==2,low=left==1&&right==1;
            if(high&&!burstHigh_){if(burstCount_<3)++burstCount_;burstHigh_=true;}
            if(low&&burstHigh_){burstHigh_=false;if(burstCount_==3)stopHazardBurst();}
          }
        }else if(burst_==Burst::Stopping&&hazardReason_!=1)cancelHazardBurst();
      }
    }
    if((id==0x2C2||id==0x2C3)&&dlc==8&&(d[0]&1)==1){const uint8_t offset=id==0x2C2?0:1;
      window_[offset]=(d[6]>>1)&7;window_[offset+2]=(d[6]>>4)&7;
      windowSeen_[offset]=windowSeen_[offset+2]=true;windowMs_[offset]=windowMs_[offset+2]=now;}
    if(id==0x294&&dlc==8){memcpy(windowTemplate_,d,8);windowTemplateSeen_=true;windowTemplateMs_=now;}
    if(id==0x273&&dlc==8){memcpy(mirrorTemplate_,d,8);mirrorTemplateSeen_=true;mirrorTemplateMs_=now;}
    const bool parked=fresh(gearSeen_,gearMs_,now,500)&&gear_==1;
    if(!parked)memset(dropped_,0,sizeof(dropped_));
    if((id==0x122||id==0x123)&&dlc==8&&(d[0]&1)==0){const uint8_t offset=id==0x122?0:1;
      for(uint8_t n=0;n<2;++n){const uint8_t i=offset+2*n;const uint8_t value=(d[2]>>(n?4:1))&7;
        if(c.easyEntry&&parked&&fresh(doorSeen_[i],doorMs_[i],now,500)&&door_[i]==4&&value==1&&dropped_[i])windowCommand(out,3,0,1u<<i,2,now);
        door_[i]=value;doorSeen_[i]=value==1||value==4;doorMs_[i]=now;
      }
    }
    if((id==0x102||id==0x103)&&dlc>=2){const uint8_t offset=id==0x102?0:1;
      for(uint8_t n=0;n<2;++n){const uint8_t i=offset+2*n;const bool value=(d[1]&(1u<<(n+2)))!=0;
        const bool doorClosed=fresh(doorSeen_[i],doorMs_[i],now,500)&&door_[i]==1;
        const bool windowClosed=fresh(windowSeen_[i],windowMs_[i],now,1000)&&(window_[i]==1||window_[i]==6);
        if(c.easyEntry&&c.windowDropRaw<=20&&parked&&doorClosed&&fresh(handleSeen_[i],handleMs_[i],now,500)){
          if(value&&!handle_[i]&&windowClosed&&fresh(weatherSeen_,weatherMs_,now,2000)&&!wet_){
            windowCommand(out,1,c.windowDropRaw,1u<<i,1,now);
            if(out.count&&c.mirrorSync&&i<2&&out.count<4&&fresh(mirrorTemplateSeen_,mirrorTemplateMs_,now,250)&&(mirrorTemplate_[3]&3)==0){auto &f=out.frames[out.count++];f.id=0x273;memcpy(f.data,mirrorTemplate_,8);f.data[3]=(f.data[3]&0xfc)|2;}
          }
          // Request 6 is undefined in the supplied DBC. No guessed STOP TX.
        }
        handle_[i]=value;handleSeen_[i]=true;handleMs_[i]=now;
      }
    }
    if(dlc!=8)return out;
    ConvenienceFrame f; f.id=id;memcpy(f.data,d,8);
    if(id==0x334&&c.kickdown){
      const bool drive=fresh(gearSeen_,gearMs_,now,500)&&gear_==4;
      const bool inputs=drive&&fresh(accelSeen_,accelMs_,now,250)&&fresh(brakeSeen_,brakeMs_,now,250);
      if(!inputs)kickApplied_=false;
      if(inputs&&brake_==1){const uint8_t stock=(d[0]>>5)&3;
        if(!kickApplied_&&stock==0&&uint16_t(accel_)*2>uint16_t(c.kickOnPct)*5)kickApplied_=true;
        if(kickApplied_&&stock==0)f.data[0]=(d[0]&uint8_t(~0x60))|0x20;
        // Sport echoes already match; a stock performance/SNA choice cancels.
        if(stock>1)kickApplied_=false;
      }
    }
    if(id==0x273&&c.reverseFog&&fresh(gearSeen_,gearMs_,now,500)&&gear_==2)f.data[2]|=0x80;
    if(id==0x3C2&&(d[0]&1)==0){
      // A physical hazard-button request takes ownership away from automation.
      // The runtime removes recent local TX echoes before this observer.
      hazardButtonSeen_=true;hazardButtonMs_=now;hazardButtonPressed_=(d[0]&8)!=0;
      if(hazardButtonPressed_)cancelHazardBurst();
      if(c.rearBeltLeft&&(d[7]&3)==2&&((d[6]>>4)&3)==1)f.data[6]=(f.data[6]&uint8_t(~0x30))|0x20;
      if(c.rearBeltCenter&&((d[6]>>6)&3)==2&&(d[7]>>6)==1)f.data[7]=(f.data[7]&uint8_t(~0xC0))|0x80;
      if(hazardPulse_&&uint32_t(now-hazardMs_)>=200){
        hazardPulse_=false;
        // Failed ON/OFF transmissions are not replayed as a later toggle.
        if(burst_==Burst::Starting||burst_==Burst::Stopping)cancelHazardBurst();
      }
      if(burst_==Burst::Starting&&uint32_t(now-burstMs_)>=1000)cancelHazardBurst();
      if(hazardRequest_&&!hazardPulse_&&(!hazardPulseSeen_||uint32_t(now-hazardMs_)>=1000)){
        hazardTarget_=hazardRequestedTarget_;hazardRequest_=false;
        if(hazardTarget_!=hazardOwned_){hazardPulse_=true;hazardPulseSeen_=true;hazardMs_=now;}
      }
      if(hazardPulse_){
        if(!fresh(gearSeen_,gearMs_,now,500)||uint32_t(now-hazardMs_)>=200)hazardPulse_=false;
        else if((d[0]&8)==0){f.data[0]|=8;f.effect=hazardTarget_?3:4;}
      }
    }
    if(id==0x3C3&&(d[0]&1)==0&&c.rearBeltRight&&((d[6]>>6)&3)==2&&((d[5]>>6)&3)==1)
      f.data[5]=(d[5]&uint8_t(~0xC0))|0x80;
    if(memcmp(f.data,d,8)!=0&&out.count<4){
      if(id==0x334)f.data[7]=teslaAdditiveChecksumPure(id,f.data);
      out.frames[out.count++]=f;
    }
    return out;
  }
 private:
  enum class Burst:uint8_t {Idle,Starting,Counting,Stopping};
  Burst burst_=Burst::Idle;
  uint32_t burstMs_=0,hazardButtonMs_=0;
  uint8_t burstCount_=0,hazardReason_=0;
  bool burstHigh_=false,lightingIdle_=false,hazardButtonSeen_=false,hazardButtonPressed_=false;
  void cancelHazardBurst(){burst_=Burst::Idle;hazardOwned_=false;hazardPulse_=false;hazardRequest_=false;}
  void stopHazardBurst(){burst_=Burst::Stopping;hazardRequest_=true;hazardRequestedTarget_=false;}
  static bool fresh(bool seen,uint32_t at,uint32_t now,uint32_t limit){return seen&&uint32_t(now-at)<=limit;}
  void windowCommand(ConvenienceOutput& out,uint8_t type,uint8_t raw,uint8_t mask,uint8_t effect,uint32_t now){
    if(out.count>=4||raw>20||type>5||!fresh(windowTemplateSeen_,windowTemplateMs_,now,250)||
       (windowTemplate_[6]&0x70)!=0)return;
    auto& f=out.frames[out.count++];f.id=0x294;
    memcpy(f.data,windowTemplate_,8);
    f.data[5]=(f.data[5]&3)|(raw<<2);f.data[6]=(f.data[6]&0x80)|(mask&15)|((type&7)<<4);f.mask=mask;f.effect=effect;
  }
  uint8_t windowTemplate_[8]={},mirrorTemplate_[8]={};
  bool windowTemplateSeen_=false,mirrorTemplateSeen_=false;
  uint32_t windowTemplateMs_=0,mirrorTemplateMs_=0;
  uint32_t epoch_=0,gearMs_=0,accelMs_=0,brakeMs_=0,weatherMs_=0,indicatorMs_=0,hazardMs_=0;
  uint8_t gear_=0,accel_=0,brake_=0,indicatorLeft_=0,indicatorRight_=0;
  bool gearSeen_=false,accelSeen_=false,brakeSeen_=false,weatherSeen_=false,indicatorSeen_=false;
  bool wet_=false,kickApplied_=false,hazardOwned_=false,hazardPulse_=false,hazardTarget_=false;
  bool hazardRequest_=false,hazardRequestedTarget_=false,hazardPulseSeen_=false;
  uint8_t door_[4]={},window_[4]={};uint32_t doorMs_[4]={},windowMs_[4]={},handleMs_[4]={};
  bool doorSeen_[4]={},windowSeen_[4]={},handleSeen_[4]={},handle_[4]={},dropped_[4]={};
};
