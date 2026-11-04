#include "../easy_entry_mirror_pure.h"
#include <cassert>
#include <cstdio>
#include <vector>
struct Fixture {
 EasyEntryMirrorEngine e;EasyEntryMirrorConfig config{true,true,20,2};
 uint32_t now=4000;uint8_t gear=1,left=2,right=2;int speed=0;
 bool accept=true;std::vector<uint8_t> requests;uint8_t last[8]={};
 Fixture(){e.configure(config);}
 void send(uint16_t id,const uint8_t*d,uint8_t dlc=8){
   uint8_t data[8]={};auto effect=e.observe(id,d,dlc,now,data);
   if(effect){assert(e.prepare(effect,now,data));requests.push_back(effect);memcpy(last,data,8);e.acknowledge(effect,accept);}
 }
 void status(bool isRight,bool handle=false){uint8_t d[8]={};d[1]=handle?4:0;d[6]=(isRight?right:left)<<4;send(isRight?0x103:0x102,d);}
 void fresh(){
   uint8_t d[8]={};d[2]=gear<<5;send(0x118,d);memset(d,0,8);
   uint16_t raw=(speed*100+4000)/8;d[5]=(raw&31)<<3;d[6]=raw>>5;send(0x257,d);
   status(false);status(true);uint8_t stock[8]={0xa5,0x5a,0x21,0xa0,0x43,0x54,0x65,0x76};send(0x273,stock);
 }
 void tick(unsigned n=1){for(unsigned i=0;i<n;++i){now+=100;fresh();}}
 void fold(){fresh();status(false,true);assert(requests.size()==1&&requests.back()==5);assert((last[3]&3)==1);assert(last[0]==0xa5&&last[3]==0xa1&&last[7]==0x76);left=right=1;fresh();}
 void start(){gear=4;speed=20;fresh();}
};
int main(){
 Fixture f;f.fold();assert(f.e.state()==EasyEntryMirrorEngine::Owned);
 f.gear=4;f.fresh();f.tick(25);assert(f.requests.size()==1); // gear selection cannot unfold
 f.speed=20;f.fresh();f.tick(19);assert(f.requests.size()==1);
 f.tick();assert(f.requests.size()==2&&f.requests.back()==6&&f.last[3]==0xa2);
 f.left=f.right=2;f.fresh();f.tick(30);assert(f.e.state()==0&&f.requests.size()==2);
 // Held handle at boot is a baseline, not an entry event. Rear handle is ignored.
 Fixture boot;boot.status(false,true);boot.fresh();uint8_t rear[8]={};rear[1]=8;rear[6]=0x20;boot.send(0x102,rear);assert(boot.requests.empty());
 for(int left:{0,1,3,4,5}){Fixture x;x.left=left;x.fresh();x.status(false,true);assert(x.requests.empty());}
 Fixture asym;asym.right=1;asym.fresh();asym.status(false,true);assert(asym.requests.empty());
 Fixture moving;moving.speed=1;moving.fresh();moving.status(false,true);assert(moving.requests.empty());
 Fixture reverse;reverse.gear=2;reverse.fresh();reverse.status(false,true);assert(reverse.requests.empty());
 Fixture off;off.config.enabled=false;off.e.configure(off.config);off.fresh();off.status(false,true);assert(off.requests.empty());
 Fixture noReturn;noReturn.config.autoReturn=false;noReturn.e.configure(noReturn.config);noReturn.fold();noReturn.start();noReturn.tick(30);assert(noReturn.requests.size()==1);
 Fixture lateEnable;lateEnable.config.autoReturn=false;lateEnable.e.configure(lateEnable.config);lateEnable.fold();lateEnable.config.autoReturn=true;lateEnable.e.configure(lateEnable.config);lateEnable.start();lateEnable.tick(30);assert(lateEnable.requests.size()==1);
 // Failure and missing physical confirmation never grant return permission.
 Fixture failed;failed.accept=false;failed.fold();assert(failed.e.state()==0);failed.start();failed.tick(30);assert(failed.requests.size()==1);
 Fixture stuck;stuck.fresh();stuck.status(false,true);stuck.tick(51);assert(stuck.e.state()==0&&stuck.requests.size()==1);
 Fixture partial;partial.fresh();partial.status(false,true);partial.left=1;partial.fresh();partial.tick(51);assert(partial.e.state()==0);
 Fixture later;later.fresh();later.status(false,true);later.gear=4;later.left=later.right=1;later.fresh();later.start();later.tick(30);assert(later.requests.size()==1);
 // Same-state fold request, unfold request, SNA, DAS and unknown feedback cancel.
 for(int request:{1,2,3}){Fixture x;x.fold();uint8_t d[8]={};d[3]=request;x.send(0x273,d);x.start();x.tick(30);assert(x.requests.size()==1);}
 Fixture das;das.fold();uint8_t d[8]={};d[3]=2;das.send(0x3e9,d);das.start();das.tick(30);assert(das.requests.size()==1);
 Fixture external;external.fold();external.left=2;external.fresh();external.left=1;external.start();external.tick(30);assert(external.requests.size()==1);
 Fixture unknown;unknown.fold();unknown.right=0;unknown.fresh();unknown.right=1;unknown.start();unknown.tick(30);assert(unknown.requests.size()==1);
 Fixture lost;lost.fold();lost.now+=501;lost.start();lost.tick(30);assert(lost.requests.size()==1);
 Fixture changed;changed.fold();changed.config.delaySec=3;changed.e.configure(changed.config);changed.start();changed.tick(40);assert(changed.requests.size()==1);
 Fixture reboot;reboot.fold();reboot.e=EasyEntryMirrorEngine{};reboot.e.configure(reboot.config);reboot.start();reboot.tick(30);assert(reboot.requests.size()==1);
 // Continuous duration: speed dip and Reverse reset the timer, without unfolding.
 Fixture dip;dip.fold();dip.start();dip.tick(19);dip.speed=19;dip.fresh();dip.speed=20;dip.fresh();dip.tick(19);assert(dip.requests.size()==1);dip.tick();assert(dip.requests.size()==2);
 Fixture r;r.fold();r.start();r.tick(19);r.gear=2;r.fresh();r.tick(30);assert(r.requests.size()==1);r.gear=4;r.fresh();r.tick(20);assert(r.requests.size()==2);
 Fixture custom;custom.config.speedKph=30;custom.config.delaySec=3;custom.e.configure(custom.config);custom.fold();custom.start();custom.tick(40);assert(custom.requests.size()==1);custom.speed=30;custom.fresh();custom.tick(29);assert(custom.requests.size()==1);custom.tick();assert(custom.requests.size()==2);
 Fixture wrap;wrap.now=UINT32_MAX-1000;wrap.fold();wrap.start();wrap.tick(20);assert(wrap.requests.size()==2);
 puts("Easy Entry mirrors: ownership, manual requests, thresholds, duration, reset, freshness and wrap PASS");
}
