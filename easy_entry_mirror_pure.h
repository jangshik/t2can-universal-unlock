#pragma once
#include <stdint.h>
#include <string.h>

struct EasyEntryMirrorConfig {
  bool enabled=false, autoReturn=false;
  uint8_t speedKph=20, delaySec=2;
};

// Return permission belongs to one continuously observed session. It is never
// restored from flash: identical folded feedback after reboot proves no history.
class EasyEntryMirrorEngine {
 public:
  enum State:uint8_t {Idle, FoldQueued, Folding, Owned, ReturnQueued, Unfolding};
  void configure(const EasyEntryMirrorConfig& c) {
    if(c.enabled!=config_.enabled||c.autoReturn!=config_.autoReturn||
       c.speedKph!=config_.speedKph||c.delaySec!=config_.delaySec){
      *this=EasyEntryMirrorEngine();config_=c;
    }
  }
  uint8_t state() const {return state_;}
  uint8_t observed(uint8_t side,uint32_t now) const {
    return side<2&&fresh(mirrorSeen_[side],mirrorMs_[side],now,500)?mirror_[side]:0;
  }
  // Effects 5/6 are local metadata, never part of the CAN payload.
  bool prepare(uint8_t effect,uint32_t now,uint8_t* data) {
    const bool valid=(effect==5?state_==FoldQueued&&parked(now)&&both(2,now):
      effect==6&&state_==ReturnQueued&&returnEligible(now)&&timing_&&
      uint32_t(now-aboveMs_)>=uint32_t(config_.delaySec)*1000);
    if(!valid||!templateIdle(now)||uint32_t(now-requestMs_)>50){cancel();return false;}
    memcpy(data,template_,8);data[3]=(data[3]&0xfc)|(effect==5?1:2);return true;
  }
  void acknowledge(uint8_t effect,bool sent) {
    if((effect==5&&state_!=FoldQueued)||(effect==6&&state_!=ReturnQueued))return;
    if(!sent){cancel();return;}
    state_=effect==5?Folding:Unfolding;
    confirmed_=0;timing_=false;
  }
  uint8_t observe(uint16_t id,const uint8_t* d,uint8_t dlc,uint32_t now,uint8_t* output) {
    // Test old freshness before updating: a recovered stream cannot resurrect
    // a return permission or count a data gap towards the speed hold interval.
    if(state_!=Idle&&(!inputsFresh(now)||!fresh(templateSeen_,templateMs_,now,500)))cancel();
    if((state_==Folding||state_==Unfolding)&&uint32_t(now-requestMs_)>5000)cancel();
    if((state_==FoldQueued||state_==ReturnQueued)&&uint32_t(now-requestMs_)>50)cancel();
    if(dlc!=8)return 0;
    bool frontEdge=false;
    if(id==0x118){gear_=(d[2]>>5)&7;gearSeen_=gear_>=1&&gear_<=4;gearMs_=now;}
    if(id==0x257){
      const uint16_t raw=(uint16_t(d[5]>>3)|(uint16_t(d[6])<<5))&0x1fff;
      speedCenti_=int32_t(raw)*8-4000;speedSeen_=raw!=8191&&speedCenti_>=0&&speedCenti_<=30000;speedMs_=now;
    }
    if(id==0x273){
      memcpy(template_,d,8);templateSeen_=true;templateMs_=now;
      // No guessed attribution/echo grace period: ANY observed non-idle stock
      // request, including an indistinguishable local echo, relinquishes control.
      if((d[3]&3)!=0)cancel();
    }
    // DAS commands can also interfere with UI mirror commands.
    if(id==0x3e9){dasRequest_=(d[3]>>1)&3;if(dasRequest_)cancel();}
    if(id==0x102||id==0x103){
      const uint8_t side=id==0x102?0:1;
      const bool handle=(d[1]&4)!=0;
      frontEdge=handle&&!handle_[side]&&fresh(handleSeen_[side],handleMs_[side],now,500);
      handle_[side]=handle;handleSeen_[side]=true;handleMs_[side]=now;
      mirror_[side]=(d[6]>>4)&7;mirrorSeen_[side]=mirror_[side]>=1&&mirror_[side]<=4;mirrorMs_[side]=now;
      if(state_==Folding){
        const uint8_t s=mirror_[side];
        if(s==0||s>3||((confirmed_&(1u<<side))&&s!=1))cancel();
        else if(s==1)confirmed_|=1u<<side;
        if(state_==Folding&&confirmed_==3&&both(1,now)){
          state_=config_.autoReturn?Owned:Idle;timing_=false;
        }
      }else if(state_==Unfolding){
        if(mirror_[side]==2)confirmed_|=1u<<side;
        else if(mirror_[side]!=1&&mirror_[side]!=4)cancel();
        if(confirmed_==3)cancel();
      }
    }
    if(!config_.enabled){cancel();return 0;}
    if((state_==Owned||state_==ReturnQueued)&&(!inputsFresh(now)||!both(1,now)))cancel();
    if(state_==Folding&&!parked(now))cancel();
    if(state_==FoldQueued&&(!parked(now)||!both(2,now)))cancel();
    if(state_==ReturnQueued&&!returnEligible(now))cancel();
    if(state_==Idle&&frontEdge&&parked(now)&&both(2,now)&&templateIdle(now)){
      state_=FoldQueued;requestMs_=now;
      if(prepare(5,now,output))return 5;
    }
    if(state_==Owned){
      if(!returnEligible(now))timing_=false;
      else {
        if(!timing_){timing_=true;aboveMs_=now;}
        if(id==0x273&&uint32_t(now-aboveMs_)>=uint32_t(config_.delaySec)*1000){
          state_=ReturnQueued;requestMs_=now;
          if(prepare(6,now,output))return 6;
        }
      }
    }
    return 0;
  }
 private:
  EasyEntryMirrorConfig config_;
  State state_=Idle;
  uint8_t mirror_[2]={},gear_=0,confirmed_=0,template_[8]={},dasRequest_=0;
  bool mirrorSeen_[2]={},handle_[2]={},handleSeen_[2]={};
  bool gearSeen_=false,speedSeen_=false,templateSeen_=false,timing_=false;
  uint32_t mirrorMs_[2]={},handleMs_[2]={},gearMs_=0,speedMs_=0,templateMs_=0,requestMs_=0,aboveMs_=0;
  int32_t speedCenti_=0;
  static bool fresh(bool seen,uint32_t at,uint32_t now,uint32_t limit){return seen&&uint32_t(now-at)<=limit;}
  void cancel(){state_=Idle;confirmed_=0;timing_=false;}
  bool inputsFresh(uint32_t now) const {
    return fresh(gearSeen_,gearMs_,now,500)&&fresh(speedSeen_,speedMs_,now,500)&&
      fresh(mirrorSeen_[0],mirrorMs_[0],now,500)&&fresh(mirrorSeen_[1],mirrorMs_[1],now,500);
  }
  bool both(uint8_t value,uint32_t now) const {
    return observed(0,now)==value&&observed(1,now)==value;
  }
  bool parked(uint32_t now) const {return inputsFresh(now)&&gear_==1&&speedCenti_<=8;}
  bool templateIdle(uint32_t now) const {
    return !dasRequest_&&fresh(templateSeen_,templateMs_,now,250)&&(template_[3]&3)==0;
  }
  bool returnEligible(uint32_t now) const {
    return config_.autoReturn&&inputsFresh(now)&&gear_==4&&both(1,now)&&
      speedCenti_>=int32_t(config_.speedKph)*100&&templateIdle(now);
  }
};
