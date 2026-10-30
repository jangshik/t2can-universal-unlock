// Reuse the real convenience HTTP/runtime fixture and action catalog stubs.
#define main convenienceFixtureRegressionMain
#include "test_convenience_api_host.cpp"
#undef main
#include "../automation_runtime.h"
#include "../automation_web.h"

static void seed(int speed,bool camera=false,uint8_t gear=4,bool stock=true){
  uint8_t d[8]={};d[2]=gear<<5;automationObserve(0,0x118,d,8,canTxBarrierState.epoch,now);
  memset(d,0,8);const uint16_t raw=(speed*100+4000)/8;d[5]=uint8_t(raw<<3);d[6]=raw>>5;
  automationObserve(0,0x257,d,8,canTxBarrierState.epoch,now);
  memset(d,0,8);d[5]=camera?2:0;automationObserve(0,0x353,d,8,canTxBarrierState.epoch,now);
  memset(d,0,8);automationObserve(0,0x294,d,8,canTxBarrierState.epoch,now);
  if(stock){d[0]=1;d[6]=0x10;automationObserve(0,0x3c2,d,8,canTxBarrierState.epoch,now);}
}
static void postRule(unsigned slot=0,unsigned action=255,unsigned enabled=1){
  server.query={{"slot",std::to_string(slot)},{"enabled",std::to_string(enabled)},{"sensor","1"},{"low","1"},{"high","20"},
    {"gear","4"},{"action",std::to_string(action)},{"trigger","0"},{"enterMs","0"},{"exitMs","3000"}};
  httpAutomationUpdate();
}
static int toggleRule(unsigned slot,unsigned enabled){
 server.query={{"slot",std::to_string(slot)},{"enabled",std::to_string(enabled)}};httpAutomationEnabled();return server.status;
}
int main(){
  // Upgrade a saved 4-rule v1 record without losing disabled rule definitions.
  AutomationConfig old;old.rules[0].enabled=true;old.rules[3].action=54;old.rules[3].high=35;
  uint8_t wire[AUTOMATION_RECORD_BYTES];automationEncode(old,wire);memcpy(autoSaved,wire,52);autoSaved[0]=1;autoSaved[1]=0;autoRecordPresent=true;
  automationLoad();assert(!automationNvsError&&automationConfig.rules[0].enabled&&automationConfig.rules[3].high==35);
  for(unsigned i=4;i<AUTOMATION_RULES;++i)assert(!automationConfig.rules[i].enabled);
  assert(toggleRule(0,0)==200&&autoV2Present);automationLoad();assert(!automationConfig.rules[0].enabled&&automationConfig.rules[3].high==35);
  automationLoad();httpAutomationConfig();assert(server.status==200&&!automationConfig.rules[0].enabled);
  postRule();assert(server.status==200&&automationConfig.rules[0].enabled&&autoV2Present);
  automationLoad();assert(automationConfig.rules[0].enabled);
  saveOk=false;postRule(0,255,0);assert(server.status==503&&automationConfig.rules[0].enabled);saveOk=true;
  postRule(1);assert(server.status==400); // Only one managed camera owner.
  postRule();server.query[0].second="256";httpAutomationUpdate();assert(server.status==400);
  postRule();server.query[2].second="257";httpAutomationUpdate();assert(server.status==400);
  postRule();server.query[9].second="30001";httpAutomationUpdate();assert(server.status==400);
  postRule();server.query[9].first="enterMs";httpAutomationUpdate();assert(server.status==400);
  bodySupported=false;postRule();assert(server.status==409);bodySupported=true;
  postRule();assert(server.status==200);automationResetUnderTxBarrier();sent.clear();
  seed(0);automationService();++now;seed(10);automationService();assert(sent.size()==1&&((sent.back().data[6]>>2)&3)==1);
  automationService();assert(sent.size()==1); // No flooding between stock RX frames.
  now+=100;seed(10,true);automationService();assert(sent.size()==2&&((sent.back().data[6]>>2)&3)==0&&automationCamera.owned());
  // Exit delay, canceled by a return into the band.
  ++now;seed(30,true);automationService();now+=100;seed(10,true);automationService();assert(sent.size()==2);
  ++now;seed(30,true);automationService();
  for(int i=0;i<29;++i){now+=100;seed(30,true);automationService();assert(sent.size()==2);}
  now+=100;seed(30,true);automationService();assert(sent.size()==3&&((sent.back().data[6]>>2)&3)==1);
  now+=100;seed(30,true);automationService();assert(sent.size()==4&&!automationCamera.owned());
  // Existing camera is never toggled or claimed.
  automationResetUnderTxBarrier();sent.clear();seed(0,true);automationService();++now;seed(10,true);automationService();assert(sent.empty()&&!automationCamera.owned());
  // Physical intervention at the final admission boundary cancels the command.
  automationResetUnderTxBarrier();seed(0);automationService();++now;seed(10);
  beforeValidate=[](){uint8_t d[8]={1,0,0,0,0,0,0x14,0};automationObserve(0,0x3c2,d,8,canTxBarrierState.epoch,now);};
  automationService();beforeValidate=nullptr;assert(sent.empty()&&!automationCamera.busy());
  // Screen activation or leaving the speed band just before TX must cancel ON.
  automationResetUnderTxBarrier();seed(0);automationService();++now;seed(10);
  beforeValidate=[](){seed(10,true);};automationService();beforeValidate=nullptr;assert(sent.empty()&&!automationCamera.busy());
  automationResetUnderTxBarrier();seed(0);automationService();++now;seed(10);
  beforeValidate=[](){seed(30);};automationService();beforeValidate=nullptr;assert(sent.empty()&&!automationCamera.busy());
  // Listen-only, transmission failure, stale RX and epoch recovery never replay.
  automationResetUnderTxBarrier();seed(0);automationService();++now;seed(10);transportOk=false;automationService();transportOk=true;assert(sent.empty());
  ++now;seed(10);automationService();assert(sent.empty());
  passive=true;automationService();passive=false;++now;seed(10);automationService();assert(sent.empty());
  ++canTxBarrierState.epoch;seed(10);automationService();assert(sent.empty());
  now+=1001;automationService();assert(!automationCamera.owned());
  // Re-entry at final OFF admission keeps the owned camera on.
  automationResetUnderTxBarrier();sent.clear();seed(0);automationService();++now;seed(10);automationService();now+=100;seed(10,true);automationService();assert(automationCamera.owned());
  ++now;seed(30,true);automationService();for(int i=0;i<29;++i){now+=100;seed(30,true);automationService();}
  now+=100;seed(30,true);beforeValidate=[](){seed(10,true);};automationService();beforeValidate=nullptr;
  assert(sent.size()==2&&automationCamera.owned()&&!automationCamera.busy());
  ++now;seed(10,true);automationService();assert(sent.size()==2&&automationCamera.owned());
  // Enable/disable preserves a saved rule and does not cancel another camera rule.
  postRule(15,54,0);assert(server.status==200&&automationCamera.owned());
  const auto definition=automationConfig.rules[15];
  assert(toggleRule(15,1)==200&&automationCamera.owned());assert(toggleRule(15,0)==200&&automationCamera.owned());
  assert(automationRuleEqual(definition,automationConfig.rules[15]));
  saveOk=false;assert(toggleRule(15,1)==503&&!automationConfig.rules[15].enabled);saveOk=true;
  assert(toggleRule(16,1)==400&&toggleRule(15,2)==400);
  assert(toggleRule(1,1)==409); // The existing active camera owner remains intact.
  assert(toggleRule(0,0)==200&&!automationCamera.owned());assert(toggleRule(0,1)==200);
  // Unsupported bus frames cannot seed the Body-only automation engine.
  automationResetUnderTxBarrier();uint8_t other[8]={};other[2]=4<<5;automationObserve(1,0x118,other,8,canTxBarrierState.epoch,now);assert(!automationSensors.gear.valid);
  // Custom actions fire once per entry and use the existing bounded dispatcher.
  postRule(0,54);assert(server.status==200);actions.clear();seed(0);automationService();++now;seed(10);automationService();assert(actions.size()==1&&actions[0]==54);
  for(int i=0;i<5;++i){now+=100;seed(10);automationService();}assert(actions.size()==1);
  // Rule 16 persists and independently produces a sensor transition action.
  postRule(0,54,0);postRule(15,54,1);assert(server.status==200);automationLoad();automationResetUnderTxBarrier();actions.clear();
  seed(0);automationService();++now;seed(10);automationService();assert(actions.size()==1&&automationActive==0x8000);
  assert(toggleRule(15,0)==200);automationLoad();assert(!automationConfig.rules[15].enabled&&automationConfig.rules[15].action==54);
  // Corrupt records are disabled without changing the convenience record.
  autoV2Saved[0]=99;automationLoad();assert(automationNvsError&&!automationConfig.rules[0].enabled);
  puts("automation API/runtime: persistence, validation, camera lifecycle, final-admission races, replay prevention PASS");
}
