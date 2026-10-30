#include "convenience_runtime_host_fixture.h"
#include <string>
#include <utility>
class String:public std::string {
 public:using std::string::string;String(const std::string&s):std::string(s){}
 bool startsWith(const char*s)const{return rfind(s,0)==0;}
 char charAt(size_t i)const{return at(i);}String substring(size_t i)const{return substr(i);}
};
struct Server {
 std::vector<std::pair<String,String>> query;int status=0;String body;
 int args(){return query.size();}String argName(int i){return query[i].first;}
 String arg(int i){return query[i].second;}
 void send(int code,const char*,String b){status=code;body=b;}
}server;
struct JsonWriterArduino {
 String&s;bool first=true;explicit JsonWriterArduino(String&o):s(o){s="{";}
 void key(const char*k){if(!first)s+=',';first=false;s+='"';s+=k;s+="\":";}
 void boolean(const char*k,bool b){key(k);s+=b?"true":"false";}
 void u32(const char*k,uint32_t v){key(k);s+=std::to_string(v);}
 void string(const char*k,const char*v){key(k);s+='"';s+=v;s+='"';}
 void raw(const char*k,const String&v){key(k);s+=v;}
 void finish(){s+='}';}
};
bool featureActionSupported(uint8_t a){return a<14||convenienceActionSupported(a);}
const char*s3xyActionCode(uint8_t a){return featureActionCode(a);}
const char*s3xyActionLabel(uint8_t a){return featureActionLabel(a);}
uint8_t featureActionLast=0;bool featureActionLastOk=true;
bool httpDecimalU16InRange(const String&s,uint16_t lo,uint16_t hi,uint16_t&out){
 if(s.empty()||s.length()>5)return false;
 uint32_t v=0;for(char c:s){if(c<'0'||c>'9')return false;v=v*10+c-'0';}
 if(v<lo||v>hi)return false;
 out=v;return true;
}
#include "../convenience_web.h"
int main(){
 convenienceLoad();httpConvenienceConfig();assert(server.status==200);
 assert(server.body.find("\"kickdown\":false")!=String::npos);
 auto post=[](std::vector<std::pair<String,String>> q){server.query=q;httpConvenienceUpdate();return server.status;};
 assert(post({{"kickdown","1"},{"kickOnPct","80"},{"kickOffPct","20"}})==200);
 assert(convenienceConfig.kickdown&&convenienceConfig.kickOnPct==80);
 assert(post({{"kickOnPct","20"}})==400&&convenienceConfig.kickOnPct==80);
 assert(post({{"reverseFog","1"},{"reverseFog","0"}})==400);
 assert(post({{"reverseFog","true"}})==400);
 assert(post({{"reverseFog","2"}})==400);
 assert(post({{"debounceMs","30junk"}})==400);
 assert(post({{"unknown","1"}})==400);
 assert(post({{"kickOnPct","65536"}})==400);
 assert(post({{"slot0Button","9"}})==400);
 assert(post({{"slot0Button","8"}})==200); // User-confirmed staged button states
 assert(convenienceMacroConfig.slots[0].button==8);
 convenienceLoad();assert(convenienceMacroConfig.slots[0].button==8);
 assert(post({{"slot0Gesture","1"}})==400);
 assert(post({{"slot0Action","255"}})==400);
 assert(post({{"slot0Button","1"},{"slot0Gesture","3"},{"slot0Action","14"},{"macrosEnabled","1"}})==200);
 assert(convenienceMacroConfig.slots[0].gesture==MacroGesture::Triple);
 saveOk=false;assert(post({{"reverseFog","1"}})==503&&!convenienceConfig.reverseFog);saveOk=true;
 assert(post({{"masterEnabled","0"},{"kickdown","0"}})==400&&!passive);
 assert(post({{"masterEnabled","0"}})==200&&passive);
 assert(post({{"masterEnabled","1"}})==200&&!passive);
 bodySupported=false;assert(post({{"reverseFog","1"}})==409);
 assert(post({{"macrosEnabled","1"}})==409); // no verified Chassis 0x3C2
 activeVehicleTopology=VEHICLE_TOPOLOGY_STANDARD_PARTY_CHASSIS;
 assert(post({{"kickdown","1"}})==200);assert(post({{"reverseFog","1"}})==409);
 bodySupported=true;activeVehicleTopology=VEHICLE_TOPOLOGY_STANDARD_BODY_CHASSIS;assert(post({{"rearBeltRight","1"}})==200);
 assert(post({{"easyEntry","1"}})==200); // user-controlled, default OFF
 httpFeatureActions();assert(server.status==200&&server.body.find("master_toggle")!=String::npos);
 // Real HTTP and runtime path: one-shot tests are separate from automatic entry.
 ConvenienceConfig testConfig;MacroGestureConfig testMacros;assert(convenienceApply(testConfig,testMacros));
 assert(!convenienceConfig.easyEntry);
 auto seed=[&](){uint8_t d[8]={};d[2]=0x20;convenienceObserve(0,0x118,d,8,canTxBarrierState.epoch,now);d[2]=0;d[0]=0xa5;convenienceObserve(0,0x294,d,8,canTxBarrierState.epoch,now);};
 auto testPost=[&](std::vector<std::pair<String,String>> args){server.query=args;httpConvenienceWindowTest();return server.status;};
 const std::vector<std::pair<String,String>> valid={{"command","1"},{"percent","100"},{"window","0"}};
 seed();auto count=sent.size();assert(testPost(valid)==200&&sent.size()==count+1);
 assert(sent.back().can_id==0x294&&sent.back().data[0]==0xa5&&sent.back().data[5]==0x50&&sent.back().data[6]==0x11);
 assert(testPost({{"command","6"},{"percent","0"},{"window","0"}})==400);
 assert(testPost({{"command","1"},{"percent","99"},{"window","0"}})==400);
 assert(testPost({{"command","1"},{"percent","100"},{"percent","0"}})==400);
 count=sent.size();labMenuEnabled=false;assert(testPost(valid)==409);labMenuEnabled=true;
 passive=true;assert(testPost(valid)==409);passive=false;
 testConfig.easyEntry=true;assert(convenienceApply(testConfig,testMacros));assert(testPost(valid)==409);
 testConfig.easyEntry=false;assert(convenienceApply(testConfig,testMacros));seed();now+=251;assert(testPost(valid)==409);
 seed();beforeValidate=[](){uint8_t d[8]={};d[2]=0x80;convenienceObserve(0,0x118,d,8,canTxBarrierState.epoch,now);};assert(testPost(valid)==409);beforeValidate=nullptr;
 assert(sent.size()==count);
 seed();beforeValidate=[](){uint8_t d[8]={};d[0]=0x5a;convenienceObserve(0,0x294,d,8,canTxBarrierState.epoch,now);};assert(testPost(valid)==200&&sent.back().data[0]==0x5a);beforeValidate=nullptr;
 httpConvenienceWindowStatus();assert(server.status==200&&server.body.find("doorAgeMs")!=String::npos);

 // One-shot action has no persistent toggle and rejects stale/busy inputs.
 convenienceResetUnderTxBarrier();
 auto hazardSeed=[&](){uint8_t d[8]={};d[2]=0x80;convenienceObserve(0,0x118,d,8,canTxBarrierState.epoch,now);d[2]=0;convenienceObserve(0,0x3f5,d,8,canTxBarrierState.epoch,now);convenienceObserve(0,0x3c2,d,8,canTxBarrierState.epoch,now);};
 server.query={};hazardSeed();const auto before=sent.size();
 httpConvenienceHazardThreeFlash();assert(server.status==200&&sent.size()==before);
 assert(convenienceEngine.hazardThreeFlashActive());
 httpConvenienceHazardThreeFlash();assert(server.status==409);
 convenienceResetUnderTxBarrier();hazardSeed();passive=true;httpConvenienceHazardThreeFlash();assert(server.status==409);passive=false;
 bodySupported=false;httpConvenienceHazardThreeFlash();assert(server.status==409);bodySupported=true;
 server.query={{"repeat","1"}};httpConvenienceHazardThreeFlash();assert(server.status==400);server.query={};
 now+=501;httpConvenienceHazardThreeFlash();assert(server.status==409);
 convenienceResetUnderTxBarrier();hazardSeed();assert(convenienceHazardThreeFlash());
 const auto beforeRace=sent.size();
 beforeValidate=[](){uint8_t d[8]={8};convenienceObserve(0,0x3c2,d,8,canTxBarrierState.epoch,now);};
 uint8_t idle[8]={};convenienceObserve(0,0x3c2,idle,8,canTxBarrierState.epoch,now);beforeValidate=nullptr;
 assert(sent.size()==beforeRace&&!convenienceEngine.hazardThreeFlashActive());
 assert(post({{"slot0Action","54"}})==200);convenienceLoad();assert(convenienceMacroConfig.slots[0].action==54);

 return 0;
}
