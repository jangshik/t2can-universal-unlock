#include "convenience_runtime_host_fixture.h"
#include <cstdio>
static void feed(uint16_t id,uint8_t*d){convenienceObserve(0,id,d,8,canTxBarrierState.epoch,now);}
static uint8_t gear=1,mirror=2;static unsigned speed=0;
static void fresh(){
 uint8_t d[8]={};d[2]=gear<<5;feed(0x118,d);memset(d,0,8);
 auto raw=(speed*100+4000)/8;d[5]=(raw&31)<<3;d[6]=raw>>5;feed(0x257,d);
 memset(d,0,8);d[6]=mirror<<4;feed(0x102,d);feed(0x103,d);
 memset(d,0,8);d[3]=0xa0;d[7]=0x76;feed(0x273,d);
}
static void handle(){uint8_t d[8]={};d[1]=4;d[6]=mirror<<4;feed(0x102,d);}
static void setup(){
 beforeValidate=nullptr;transportOk=true;passive=false;bodySupported=true;gear=1;speed=0;mirror=2;now+=1000;
 ConvenienceConfig c;c.mirrorSync=c.mirrorAutoReturn=true;
 assert(convenienceApply(c,{}));convenienceResetUnderTxBarrier();sent.clear();fresh();
}
static void own(){handle();assert(sent.size()==1&&sent.back().data[3]==0xa1);mirror=1;fresh();assert(convenienceEngine.mirrorReturnState()==3);}
int main(){
 // Final transport admission checks changes that arrive after output generation.
 for(int race=0;race<5;++race){
   setup();
   if(race==0)beforeValidate=[](){beforeValidate=nullptr;gear=4;fresh();};
   if(race==1)beforeValidate=[](){beforeValidate=nullptr;uint8_t d[8]={};d[3]=1;feed(0x273,d);};
   if(race==2)beforeValidate=[](){beforeValidate=nullptr;now+=501;};
   if(race==3)beforeValidate=[](){beforeValidate=nullptr;mirror=1;fresh();};
   if(race==4)transportOk=false;
   handle();beforeValidate=nullptr;assert(sent.empty()&&convenienceEngine.mirrorReturnState()==0);
 }
 for(int race=0;race<5;++race){
   setup();own();gear=4;speed=20;fresh();for(int i=0;i<19;++i){now+=100;fresh();}
   // Return is generated only on idle 0x273, then the callback races admission.
   if(race==0)beforeValidate=[](){beforeValidate=nullptr;speed=19;fresh();};
   if(race==1)beforeValidate=[](){beforeValidate=nullptr;uint8_t d[8]={};d[3]=1;feed(0x273,d);};
   if(race==2)beforeValidate=[](){beforeValidate=nullptr;convenienceResetUnderTxBarrier();};
   if(race==3)beforeValidate=[](){beforeValidate=nullptr;auto c=convenienceConfig;c.mirrorAutoReturn=false;assert(convenienceApply(c,{}));};
   if(race==4)transportOk=false;
   now+=100;fresh();beforeValidate=nullptr;assert(sent.size()==1);
   transportOk=true;for(int i=0;i<30;++i){now+=100;fresh();}assert(sent.size()==1);
 }
 setup();own();gear=4;speed=20;fresh();for(int i=0;i<20;++i){now+=100;fresh();}
 assert(sent.size()==2&&sent.back().data[3]==0xa2&&sent.back().data[7]==0x76);
 // Settings persist, return permission does not. v1 entry-unfold migrates OFF.
 setup();own();convenienceLoad();assert(convenienceConfig.mirrorSync&&convenienceConfig.mirrorAutoReturn&&convenienceEngine.mirrorReturnState()==0);
 saved[0]=1;memset(saved+23,0,9);convenienceLoad();assert(!convenienceNvsError&&!convenienceConfig.mirrorSync&&!convenienceConfig.mirrorAutoReturn);
 assert(convenienceConfig.mirrorReturnSpeedKph==20&&convenienceConfig.mirrorReturnDelaySec==2);
 auto c=convenienceConfig;c.mirrorSync=c.mirrorAutoReturn=true;c.mirrorReturnSpeedKph=25;c.mirrorReturnDelaySec=3;
 assert(convenienceApply(c,{}));convenienceLoad();assert(convenienceConfig.mirrorReturnSpeedKph==25&&convenienceConfig.mirrorReturnDelaySec==3);
 saved[24]=0;convenienceLoad();assert(convenienceNvsError&&!convenienceConfig.mirrorSync);
 puts("Easy Entry mirrors runtime: transport races, failed TX, migration and conservative reboot PASS");
}
