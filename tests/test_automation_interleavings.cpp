#define main convenienceFixtureMain
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
static void ownCamera(){
 postRule();automationResetUnderTxBarrier();sent.clear();seed(0);automationService();++now;seed(10);automationService();
 now+=100;seed(10,true);automationService();assert(automationCamera.owned()&&sent.size()==2);
}
int main(int argc,char**argv){
 assert(argc==2);const std::string scenario=argv[1];
 if(scenario=="camera_off_on"){
  ownCamera();++now;seed(10,false);++now;seed(10,true);automationService();
  assert(!automationCamera.owned()); // OFF/ON between web ticks is manual intervention.
 }else if(scenario=="camera_gear"){
  ownCamera();++now;seed(10,true,2);++now;seed(10,true,4);automationService();assert(!automationCamera.owned());
 }else if(scenario=="camera_rx_gap"){
  ownCamera();now+=501;seed(10,true);automationService();assert(!automationCamera.owned());
 }else if(scenario=="camera_service_gap"){
  ownCamera();++now;seed(30,true);automationService();
  for(unsigned i=0;i<6;++i){now+=100;seed(30,true);} // RX healthy while web work stalls
  automationService();assert(!automationCamera.owned()&&sent.size()==2);
 }else if(scenario=="rule_service_gap"){
  AutomationConfig c;c.rules[0].enabled=true;c.rules[0].action=54;c.rules[0].enterMs=100;
  AutomationEngine e;AutomationSensors s;s.gear.set(4,100);s.speed.set(0,100);e.evaluate(c,s,100);
  s.speed.set(1000,101);e.evaluate(c,s,101);
  s.gear.set(4,1000);s.speed.set(1000,1000);assert(!e.evaluate(c,s,1000).entered);
 }else if(scenario=="sensor_gap"||scenario=="sensor_invalid"){
  AutomationConfig c;c.rules[0].enabled=true;c.rules[0].action=54;c.rules[0].enterMs=500;
  AutomationEngine e;AutomationSensors s;s.gear.set(4,100);s.speed.set(0,100);e.evaluate(c,s,100);
  s.speed.set(1000,101);e.evaluate(c,s,101);
  s.gear.set(4,500);e.evaluate(c,s,500);
  if(scenario=="sensor_invalid")s.speed.set(0,501,false);
  s.speed.set(1000,650);s.gear.set(4,650);assert(!e.evaluate(c,s,650).entered);
 }else if(scenario=="concurrent_toggles"){
  actions.clear();assert(featureActionEnqueue(FEATURE_ACTION_KICKDOWN));assert(featureActionEnqueue(FEATURE_ACTION_REVERSE_FOG));
  const auto first=actions.front();actions.erase(actions.begin());assert(convenienceToggleAction(first));
  assert(actions.size()==1&&actions[0]==FEATURE_ACTION_REVERSE_FOG);
  assert(convenienceToggleAction(actions[0]));assert(convenienceConfig.kickdown&&convenienceConfig.reverseFog);
 }else if(scenario=="disable_queued_rule"){
  postRule(0,54);actions.clear();seed(0);automationService();++now;seed(10);automationService();assert(actions.size()==1);
  auto same=automationConfig;assert(automationApply(same));assert(actions.size()==1); // idempotent save does not cancel
  saveOk=false;assert(toggleRule(0,0)==503&&actions.size()==1);saveOk=true;
  assert(toggleRule(0,0)==200);assert(actions.empty());
 }else return 2;
 printf("interleaving %s PASS\n",argv[1]);
}
