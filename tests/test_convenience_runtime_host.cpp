#include "convenience_runtime_host_fixture.h"
static void feed(uint16_t id,uint8_t*d,uint8_t bus=0){convenienceObserve(bus,id,d,8,canTxBarrierState.epoch,now);}
static void drive(){uint8_t d[8]={};d[2]=4<<5;d[4]=250;feed(0x118,d);d[2]=1;feed(0x39d,d);}
int main(){
 convenienceLoad();assert(!convenienceConfig.kickdown&&!convenienceMacroConfig.enabled);
 ConvenienceConfig c;c.kickdown=true;MacroGestureConfig m;
 assert(convenienceApply(c,m));assert(recordPresent);
 convenienceConfig={};convenienceLoad();assert(convenienceConfig.kickdown);
 saveOk=false;c.kickdown=false;const auto generation=convenienceGeneration;
 assert(!convenienceApply(c,m));assert(convenienceConfig.kickdown&&generation==convenienceGeneration);saveOk=true;
 uint8_t raw[8]={};drive();feed(0x334,raw);assert(sent.size()==1&&sent.back().data[0]==0x20);
 passive=true;feed(0x334,raw);assert(sent.size()==1);passive=false;
 drive();beforeValidate=[](){twaiReady=false;};feed(0x334,raw);assert(sent.size()==1);beforeValidate=nullptr;twaiReady=true;
 drive();beforeValidate=[](){canTxBarrierState.epoch++;};feed(0x334,raw);assert(sent.size()==1);beforeValidate=nullptr;
 drive();pedalMapSession.active=true;feed(0x334,raw);assert(sent.size()==1);pedalMapSession.active=false;
 drive();apDrive=true;feed(0x334,raw);assert(sent.size()==1);apDrive=false;
 drive();beforeValidate=[](){pedalMapSession.active=true;};feed(0x334,raw);assert(sent.size()==1);beforeValidate=nullptr;pedalMapSession.active=false;
 bodySupported=false;assert(!convenienceActionSupported(FEATURE_ACTION_KICKDOWN));assert(!convenienceActionSupported(FEATURE_ACTION_MACRO));
 drive();feed(0x334,raw,1);assert(sent.size()==1); // no Chassis injection by numeric ID
 assert(convenienceToggleAction(FEATURE_ACTION_MASTER)&&passive);assert(convenienceToggleAction(FEATURE_ACTION_MASTER)&&!passive);
 bodySupported=true;c={};c.rearBeltLeft=true;assert(convenienceApply(c,m));raw[6]=0x10;raw[7]=2;feed(0x3c2,raw);assert(sent.size()==2&&(sent.back().data[6]&0x30)==0x20);
 // Invalid saved configuration is disabled, never partially applied.
 saved[0]=99;convenienceLoad();assert(convenienceNvsError&&!convenienceConfig.rearBeltLeft&&!convenienceMacroConfig.enabled);
 // Versioned persistence retains all gesture fields.
 c={};m.enabled=true;m.slots[1]={1,MacroGesture::Triple,FEATURE_ACTION_MASTER};m.longPressMs=1234;
 assert(convenienceApply(c,m));convenienceLoad();assert(convenienceMacroConfig.slots[1].action==FEATURE_ACTION_MASTER&&convenienceMacroConfig.longPressMs==1234);
 // Supplied DBC confirms 0x3C2 on Body only.
 bodySupported=false;uint8_t d[8]={0x29,0x55,0x40,0,0,0x40,0x15,0};
 convenienceObserve(0,0x3c2,d,8,canTxBarrierState.epoch,now);
 assert(convenienceMacros.config().enabled);
 assert(!convenienceMacroSupported());
 bodySupported=true;
 auto tick=[&](bool pressed){d[1]=(d[1]&0xcf)|(pressed?0x20:0x10);feed(0x3c2,d,0);convenienceService();};
 // Listen-only still observes physical inputs, so a Master macro can restore TX.
 passive=true;tick(false);now+=40;tick(false);
 for(unsigned n=0;n<3;++n){now+=40;tick(true);now+=40;tick(true);now+=40;tick(false);now+=40;tick(false);}
 assert(actions.size()==1&&actions[0]==FEATURE_ACTION_MASTER);actions.clear();passive=false;

 // Chassis-only kickdown is independent of Body functions and preserves its
 // own stock counter/checksum. Party CAN A can never masquerade as Chassis.
 passive=false;bodySupported=false;activeVehicleTopology=VEHICLE_TOPOLOGY_STANDARD_PARTY_CHASSIS;
 c={};c.kickdown=true;m={};assert(convenienceApply(c,m));convenienceResetUnderTxBarrier();
 assert(convenienceActionSupported(FEATURE_ACTION_KICKDOWN));assert(!convenienceMacroSupported());
 auto chassisDrive=[&](){uint8_t in[8]={};in[2]=0x80;in[4]=250;feed(0x118,in,1);in[2]=1;feed(0x39d,in,1);};
 uint8_t stock[8]={};chassisDrive();feed(0x334,stock,0);assert(chassisSent.empty());
 feed(0x334,stock,1);assert(chassisSent.size()==1&&chassisSent[0].data[0]==0x20&&chassisSent[0].data[7]==0x57);
 passive=true;feed(0x334,stock,1);assert(chassisSent.size()==1);passive=false;
 chassisDrive();beforeValidate=[](){activeVehicleTopology=VEHICLE_TOPOLOGY_STANDARD_BODY_CHASSIS;};feed(0x334,stock,1);assert(chassisSent.size()==1);beforeValidate=nullptr;
 activeVehicleTopology=VEHICLE_TOPOLOGY_STANDARD_PARTY_CHASSIS;
 chassisDrive();beforeValidate=[](){pedalMapSession.active=true;};feed(0x334,stock,1);assert(chassisSent.size()==1);beforeValidate=nullptr;pedalMapSession.active=false;
 chassisDrive();beforeValidate=[](){canTxBarrierState.epoch++;};feed(0x334,stock,1);assert(chassisSent.size()==1);beforeValidate=nullptr;
 // Reset cancels queued macros at recovery boundary.
 actions.push_back(FEATURE_ACTION_MASTER);convenienceResetUnderTxBarrier();assert(actions.empty());
 // Automatic entry is enabled by its persisted toggle, independent of LAB.
 bodySupported=true;activeVehicleTopology=VEHICLE_TOPOLOGY_STANDARD_BODY_CHASSIS;labMenuEnabled=false;
 c={};c.easyEntry=true;m=MacroGestureConfig{};assert(convenienceApply(c,m));convenienceResetUnderTxBarrier();
 uint8_t in[8]={};in[2]=0x20;feed(0x118,in);in[2]=0;feed(0x61d,in);feed(0x294,in);
 in[0]=1;in[6]=2;feed(0x2c2,in);in[0]=0;in[6]=0;in[2]=2;feed(0x122,in);
 in[2]=0;feed(0x102,in);auto beforeEntry=sent.size();in[1]=4;feed(0x102,in);
 assert(sent.size()==beforeEntry+1&&sent.back().can_id==0x294&&sent.back().data[5]==0x50);
 c.easyEntry=false;assert(convenienceApply(c,m));in[1]=0;feed(0x102,in);in[1]=4;feed(0x102,in);assert(sent.size()==beforeEntry+1);

}
