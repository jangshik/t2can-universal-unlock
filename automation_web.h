#pragma once
static void httpAutomationConfig(){
  AutomationConfig c;AutomationSensors sensors;AutomationMask active,invalid;uint8_t camera;uint32_t actions,rejected;bool nvsError;
  portENTER_CRITICAL(&automationMux);
  c=automationConfig;sensors=automationSensors;active=automationActive;invalid=automationInvalid;camera=automationCamera.status();
  actions=automationActions;rejected=automationRejected;nvsError=automationNvsError;
  portEXIT_CRITICAL(&automationMux);
  const uint32_t now=millis();String out;JsonWriterArduino w(out);
  w.u32("capacity",AUTOMATION_RULES);
  w.boolean("supported",convenienceBodySupported());w.boolean("running",automationAllowed());w.boolean("nvsError",nvsError);
  w.u32("active",active);w.u32("invalid",invalid);w.u32("cameraState",camera);w.u32("actions",actions);w.u32("rejected",rejected);
  w.boolean("speedFresh",sensors.speed.fresh(now));w.u32("speedCenti",sensors.speed.value>0?sensors.speed.value:0);
  w.boolean("gearFresh",sensors.gear.fresh(now));w.u32("gear",sensors.gear.value);
  w.boolean("brakeFresh",sensors.brake.fresh(now));w.u32("brake",sensors.brake.value);
  w.boolean("cameraFresh",sensors.cameraFresh(now));w.boolean("cameraOn",sensors.cameraOn());
  String rules="[";
  for(unsigned i=0;i<AUTOMATION_RULES;++i){if(i)rules+=',';const auto&r=c.rules[i];String item;JsonWriterArduino j(item);
    j.boolean("enabled",r.enabled);j.u32("sensor",r.sensor);j.u32("low",r.low);j.u32("high",r.high);j.u32("gear",r.gear);
    j.u32("action",r.action);j.u32("trigger",r.trigger);j.u32("enterMs",r.enterMs);j.u32("exitMs",r.exitMs);j.finish();rules+=item;
  }
  rules+=']';w.raw("rules",rules);w.raw("featureActions",featureActionsJsonArray());w.finish();server.send(200,"application/json",out);
}
static void httpAutomationUpdate(){
  // Save one complete rule atomically; do not partially persist editor changes.
  if(server.args()!=10){server.send(400,"application/json","{\"error\":\"complete rule required\"}");return;}
  const char*keys[]={"slot","enabled","sensor","low","high","gear","action","trigger","enterMs","exitMs"};
  uint16_t values[10]={};uint16_t seen=0;
  for(int n=0;n<server.args();++n){
    int field=-1;for(int i=0;i<10;++i)if(server.argName(n)==keys[i])field=i;
    uint16_t value=0;
    if(field<0||(seen&(1u<<field))||!httpDecimalU16InRange(server.arg(n),0,30000,value)){
      server.send(400,"application/json","{\"error\":\"invalid or duplicate field\"}");return;
    }
    seen|=1u<<field;values[field]=value;
  }
  if(values[0]>=AUTOMATION_RULES||values[1]>1||values[2]>3||values[3]>150||values[4]>150||values[5]>4||values[6]>255||values[7]>1){
    server.send(400,"application/json","{\"error\":\"value out of range\"}");return;
  }
  AutomationConfig c;portENTER_CRITICAL(&automationMux);c=automationConfig;portEXIT_CRITICAL(&automationMux);
  auto&r=c.rules[values[0]];r.enabled=values[1];r.sensor=values[2];r.low=values[3];r.high=values[4];r.gear=values[5];
  r.action=values[6];r.trigger=values[7];r.enterMs=values[8];r.exitMs=values[9];
  if(!automationConfigValid(c,FEATURE_ACTION_LAST)){
    server.send(400,"application/json","{\"error\":\"Invalid condition. Use one camera rule, Speed + Drive + Enter.\"}");return;
  }
  if(r.enabled&&(!convenienceBodySupported()||(r.action!=AUTOMATION_CAMERA&&!featureActionSupported(r.action)))){
    server.send(409,"application/json","{\"error\":\"action unavailable for this connection\"}");return;
  }
  if(!automationApply(c)){server.send(503,"application/json","{\"error\":\"save failed\"}");return;}
  httpAutomationConfig();
}

static void httpAutomationEnabled(){
  if(server.args()!=2){server.send(400,"application/json","{\"error\":\"slot and enabled required\"}");return;}
  uint16_t slot=0,enabled=0;unsigned seen=0;
  for(int n=0;n<2;++n){
    const String key=server.argName(n);uint16_t value=0;const unsigned bit=key=="slot"?1:(key=="enabled"?2:0);
    if(!bit||(seen&bit)||!httpDecimalU16InRange(server.arg(n),0,bit==1?AUTOMATION_RULES-1:1,value)){
      server.send(400,"application/json","{\"error\":\"invalid toggle\"}");return;
    }
    seen|=bit;if(bit==1)slot=value;else enabled=value;
  }
  AutomationConfig c;portENTER_CRITICAL(&automationMux);c=automationConfig;portEXIT_CRITICAL(&automationMux);
  auto&r=c.rules[slot];r.enabled=enabled;
  if(!automationConfigValid(c,FEATURE_ACTION_LAST)){
    server.send(409,"application/json","{\"error\":\"Only one automatic camera rule may be enabled\"}");return;
  }
  if(r.enabled&&(!convenienceBodySupported()||(r.action!=AUTOMATION_CAMERA&&!featureActionSupported(r.action)))){
    server.send(409,"application/json","{\"error\":\"action unavailable for this connection\"}");return;
  }
  if(!automationApply(c)){server.send(503,"application/json","{\"error\":\"save failed\"}");return;}
  httpAutomationConfig();
}
