#define main convenienceApiFixtureMain
#include "test_convenience_api_host.cpp"
#undef main
#include <cstdio>

static void feed(uint16_t id,const uint8_t* d,uint8_t dlc=8){
 convenienceObserve(0,id,d,dlc,canTxBarrierState.epoch,now);
}
static void weather(uint8_t bits,uint8_t dlc=8){uint8_t d[8]={};d[7]=bits;feed(0x61d,d,dlc);}
static void fresh(uint8_t gear=1,unsigned speed=0,uint8_t mirror=2){
 uint8_t d[8]={};d[2]=gear<<5;feed(0x118,d);memset(d,0,8);
 const auto raw=(speed*100+4000)/8;d[5]=(raw&31)<<3;d[6]=raw>>5;feed(0x257,d);
 memset(d,0,8);feed(0x273,d);feed(0x294,d);
 d[0]=1;d[6]=0x12;feed(0x2c2,d);feed(0x2c3,d);
 memset(d,0,8);d[2]=0x12;feed(0x122,d);feed(0x123,d);
 memset(d,0,8);d[6]=mirror<<4;feed(0x102,d);feed(0x103,d);
}
static void handle(bool right=false,bool pulled=true,uint8_t mirror=2){
 uint8_t d[8]={};d[1]=pulled?4:0;d[6]=mirror<<4;feed(right?0x103:0x102,d);
}
static void setup(bool guard=true,bool window=true,bool mirror=true){
 beforeValidate=nullptr;saveOk=transportOk=bodySupported=true;passive=false;now+=3000;
 ConvenienceConfig c;c.easyEntry=window;c.mirrorSync=mirror;c.mirrorAutoReturn=true;c.easyEntryWeatherGuard=guard;
 assert(convenienceApply(c,{}));convenienceResetUnderTxBarrier();sent.clear();
}
static size_t count(uint16_t id){size_t n=0;for(const auto& f:sent)if(f.can_id==id)++n;return n;}
static void matrix(){
 // Both front handles; rain, snow, both, unknown/malformed and stale weather.
 for(bool right:{false,true})for(bool guard:{false,true})for(int sample=0;sample<7;++sample){
   setup(guard);
   if(sample<4)weather(sample);
   if(sample==5)weather(0,7);
   if(sample==6){weather(0);now+=2001;}
   fresh();handle(right);
   const size_t expected=(!guard||sample==0)?1:0;
   assert(count(0x294)==expected&&count(0x273)==expected);
 }
 // Freshness is inclusive at 2 s. Guard OFF does not remove Park checks.
 setup();weather(0);now+=2000;fresh();handle();assert(sent.size()==2);
 setup(false);weather(3);fresh(4);handle();assert(sent.empty());
 // Guarded handles are consumed, never replayed when the weather clears.
 setup();weather(1);fresh();handle();assert(sent.empty());
 weather(0);handle();assert(sent.empty());handle(false,false);handle();assert(sent.size()==2);
 // Enabling/disabling the guard is not a synthetic handle event.
 setup();weather(2);fresh();handle();auto c=convenienceConfig;c.easyEntryWeatherGuard=false;
 assert(convenienceApply(c,{}));handle();assert(sent.empty());handle(false,false);handle();assert(sent.size()==2);
 // Weather freshness arithmetic survives millis wrap.
 setup();bootTime=UINT32_MAX-10000;now=UINT32_MAX-100;weather(0);now=50;fresh();handle();assert(sent.size()==2);
 bootTime=0;now=10000;
}
static void finalAdmission(){
 // Actual runtime path: new rain/snow after output construction, before TX.
 for(bool guard:{false,true})for(bool mirror:{false,true})for(uint8_t wet:{1,2,3}){
   setup(guard,!mirror,mirror);weather(0);fresh();
   static uint8_t changed;changed=wet;
   beforeValidate=[](){beforeValidate=nullptr;weather(changed);};
   handle();assert(sent.size()==(guard?0u:1u));
   if(guard&&mirror)assert(convenienceEngine.mirrorReturnState()==0);
 }
 // A 1 ms admission delay can expire weather while command age remains valid.
 setup();weather(0);now+=2000;fresh();beforeValidate=[](){beforeValidate=nullptr;++now;};
 handle();assert(sent.empty());
}
static void restoration(){
 // Owned window close remains allowed in rain/snow and without fresh weather.
 for(bool stale:{false,true}){
   setup(true,true,false);weather(0);fresh();handle();assert(count(0x294)==1);
   for(int tick=0;tick<22;++tick){now+=100;fresh();if(!stale)weather(3);}
   uint8_t d[8]={};d[2]=0x18;feed(0x122,d);d[2]=0x12;feed(0x122,d);
   assert(count(0x294)==2&&(sent.back().data[6]&0x70)==0x30);
 }
 // Mirror return stays armed through wet/stale weather and uses its speed timer.
 for(bool stale:{false,true}){
   setup(true,false,true);weather(0);fresh();handle();assert(count(0x273)==1);
   fresh(1,0,1);assert(convenienceEngine.mirrorReturnState()==3);
   fresh(4,20,1);
   for(int tick=0;tick<21;++tick){now+=100;if(!stale)weather(3);fresh(4,20,1);}
   assert(count(0x273)==2&&(sent.back().data[3]&3)==2);
 }
}
static void persistenceAndApi(){
 setup();assert(saved[0]==3&&saved[26]==1);convenienceLoad();assert(convenienceConfig.easyEntryWeatherGuard);
 auto post=[](std::vector<std::pair<String,String>> q){server.query=q;httpConvenienceUpdate();return server.status;};
 assert(post({{"easyEntryWeatherGuard","0"}})==200);
 assert(server.body.find("\"easyEntryWeatherGuard\":false")!=String::npos);
 convenienceLoad();assert(!convenienceConfig.easyEntryWeatherGuard);
 saveOk=false;assert(post({{"easyEntryWeatherGuard","1"}})==503&&!convenienceConfig.easyEntryWeatherGuard);saveOk=true;
 assert(post({{"easyEntryWeatherGuard","true"}})==400);
 assert(post({{"easyEntryWeatherGuard","2"}})==400);
 assert(post({{"easyEntryWeatherGuard","0"},{"easyEntryWeatherGuard","1"}})==400);
 bodySupported=false;assert(post({{"easyEntryWeatherGuard","1"}})==409);bodySupported=true;
 // v2 preserves mirror settings; v1 retains its conservative mirror migration.
 for(uint8_t version:{1,2}){
   setup(false);saved[0]=version;memset(saved+(version==1?23:26),0,version==1?9:6);
   convenienceLoad();assert(!convenienceNvsError&&convenienceConfig.easyEntryWeatherGuard);
   assert(convenienceConfig.mirrorSync==(version==2));
 }
 setup(false);saved[26]=2;convenienceLoad();assert(convenienceNvsError&&convenienceConfig.easyEntryWeatherGuard&&!convenienceConfig.easyEntry);
 setup(false);saved[27]=1;convenienceLoad();assert(convenienceNvsError);
}
int main(){matrix();finalAdmission();restoration();persistenceAndApi();puts("Easy Entry weather: window/mirror matrix, TX races, restoration, migration and API PASS");}
