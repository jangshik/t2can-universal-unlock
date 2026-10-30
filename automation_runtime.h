#pragma once
#include "automation_pure.h"

static portMUX_TYPE automationMux=portMUX_INITIALIZER_UNLOCKED;
static AutomationConfig automationConfig;
static AutomationSensors automationSensors;
static AutomationEngine automationEngine;
static AutomationCamera automationCamera;
static uint32_t automationEpoch=0,automationGeneration=1,automationStockSerial=0,automationSentSerial=0;
static uint32_t automationActions=0,automationRejected=0;
static bool automationNvsError=false;
static AutomationMask automationActive=0,automationInvalid=AUTOMATION_ALL;

static void automationResetLocked(){
  automationSensors=AutomationSensors{};automationEngine.reset();automationCamera.reset();
  automationActive=0;automationInvalid=AUTOMATION_ALL;automationStockSerial=automationSentSerial=0;
  ++automationGeneration;
}
static void automationResetUnderTxBarrier(){
  portENTER_CRITICAL(&automationMux);automationEpoch=canTxBarrierState.epoch;automationResetLocked();portEXIT_CRITICAL(&automationMux);
}
static void automationEncode(const AutomationConfig& c,uint8_t b[AUTOMATION_RECORD_BYTES]){
  memset(b,0,AUTOMATION_RECORD_BYTES);b[0]=2;b[1]=AUTOMATION_RULES;
  for(unsigned i=0;i<AUTOMATION_RULES;++i){const auto&r=c.rules[i];auto*p=b+4+i*12;
    p[0]=r.enabled;p[1]=r.sensor;p[2]=r.low;p[3]=r.high;p[4]=r.gear;p[5]=r.action;p[6]=r.trigger;
    p[8]=r.enterMs&255;p[9]=r.enterMs>>8;p[10]=r.exitMs&255;p[11]=r.exitMs>>8;
  }
}
static bool automationDecode(const uint8_t*b,unsigned size,AutomationConfig& c){
  const bool legacy=size==64&&b[0]==1;
  const unsigned count=legacy?4:AUTOMATION_RULES;
  if((legacy?b[1]!=0:(size!=AUTOMATION_RECORD_BYTES||b[0]!=2||b[1]!=AUTOMATION_RULES))||b[2]||b[3])return false;
  for(unsigned i=4+count*12;i<size;++i)if(b[i])return false;
  c=AutomationConfig{};
  for(unsigned i=0;i<count;++i){auto&r=c.rules[i];const auto*p=b+4+i*12;
    if(p[0]>1||p[7])return false;
    r.enabled=p[0];r.sensor=p[1];r.low=p[2];r.high=p[3];r.gear=p[4];r.action=p[5];r.trigger=p[6];
    r.enterMs=uint16_t(p[8])|(uint16_t(p[9])<<8);r.exitMs=uint16_t(p[10])|(uint16_t(p[11])<<8);
  }
  return automationConfigValid(c,FEATURE_ACTION_LAST);
}
static void automationLoad(){
  AutomationConfig c;uint8_t b[AUTOMATION_RECORD_BYTES]={};bool found=false;
  automationNvsError=!nvsCheckedReadBlob("features","automation_v2",b,sizeof(b),found);
  if(!automationNvsError&&found)automationNvsError=!automationDecode(b,sizeof(b),c);
  else if(!automationNvsError){
    // Preserve the four previously saved rules. Never resurrect a legacy record
    // when a present v2 record is corrupt; fall back only when v2 is absent.
    uint8_t legacy[64]={};
    automationNvsError=!nvsCheckedReadBlob("features","automation",legacy,sizeof(legacy),found);
    if(!automationNvsError&&found)automationNvsError=!automationDecode(legacy,sizeof(legacy),c);
  }
  if(automationNvsError)c=AutomationConfig{};
  portENTER_CRITICAL(&automationMux);automationConfig=c;automationResetLocked();portEXIT_CRITICAL(&automationMux);
}
static bool automationApply(const AutomationConfig& c){
  if(!automationConfigValid(c,FEATURE_ACTION_LAST)||!canTxBarrierMutex||xSemaphoreTake(canTxBarrierMutex,pdMS_TO_TICKS(500))!=pdTRUE)return false;
  uint8_t b[AUTOMATION_RECORD_BYTES];automationEncode(c,b);const bool ok=nvsCheckedWriteBlob("features","automation_v2",b,sizeof(b));
  if(ok){
    portENTER_CRITICAL(&automationMux);
    for(unsigned i=0;i<AUTOMATION_RULES;++i)if(!automationRuleEqual(c.rules[i],automationConfig.rules[i])){
      automationEngine.resetRule(i);const AutomationMask bit=AutomationMask(1u<<i);automationActive&=~bit;automationInvalid|=bit;
      if((c.rules[i].enabled&&c.rules[i].action==AUTOMATION_CAMERA)||
         (automationConfig.rules[i].enabled&&automationConfig.rules[i].action==AUTOMATION_CAMERA))automationCamera.reset();
    }
    automationConfig=c;automationNvsError=false;++automationGeneration;
    portEXIT_CRITICAL(&automationMux);
  }
  xSemaphoreGive(canTxBarrierMutex);return ok;
}
static bool automationAllowed(){
  return convenienceBodySupported()&&!canUsbTxHeld()&&!canTxAdministrativeHold&&!canMaintenanceActive()&&
    !__atomic_load_n(&canUsbTransitionHeld,__ATOMIC_ACQUIRE)&&mcpReady&&twaiReady&&uint32_t(millis()-bootTime)>=3000;
}
static void automationObserve(uint8_t bus,uint16_t id,const uint8_t*d,uint8_t dlc,uint32_t epoch,uint32_t at){
  if(bus!=0||!convenienceBodySupported()||!d||dlc!=8)return;
  if(id==0x3c2&&canSteeringRecentTxEcho(bus,d,dlc,at))return;
  portENTER_CRITICAL(&automationMux);
  if(automationEpoch!=epoch){automationEpoch=epoch;automationResetLocked();}
  automationSensors.observe(id,d,dlc,at);automationCamera.observe(id,d,dlc,at);
  if(id==0x3c2&&(d[0]&1))++automationStockSerial;
  portEXIT_CRITICAL(&automationMux);
}
static bool automationCameraConditionLocked(){
  for(const auto&r:automationConfig.rules)if(r.enabled&&r.action==AUTOMATION_CAMERA)
    return automationSensors.gear.value==4&&automationSensors.speed.value>=int32_t(r.low)*100&&automationSensors.speed.value<=int32_t(r.high)*100;
  return false;
}
struct AutomationTxContext{uint32_t generation,serial;uint8_t kind=0;bool canceled=false;};
static bool automationValidateCamera(struct can_frame*f,void*arg){
  auto&ctx=*static_cast<AutomationTxContext*>(arg);
  if(!automationAllowed()||convenienceSteeringBusy())return false;
  portENTER_CRITICAL(&automationMux);
  automationCamera.maintain(automationSensors,millis(),true);
  const bool match=automationCameraConditionLocked();
  if(automationCamera.startingOpen()&&!match)automationCamera.reset();
  if(automationCamera.startingClose()&&match){automationCamera.cancelUnsentClose();ctx.canceled=true;}
  bool ok=ctx.generation==automationGeneration&&automationEpoch==canTxBarrierState.epoch&&automationStockSerial!=automationSentSerial;
  if(ok){ctx.kind=automationCamera.prepare(f->data,millis());ctx.serial=automationStockSerial;ok=ctx.kind!=0;}
  portEXIT_CRITICAL(&automationMux);
  f->can_id=0x3c2;f->can_dlc=8;return ok;
}
static void automationService(){
  const uint32_t now=millis();const bool allowed=automationAllowed(),cameraAllowed=allowed&&!convenienceSteeringBusy();
  uint8_t actions[AUTOMATION_RULES]={};unsigned count=0;uint32_t epoch=0;AutomationTxContext ctx{};bool tx=false;
  portENTER_CRITICAL(&automationMux);
  if(!allowed||automationEpoch!=canTxBarrierState.epoch){automationEpoch=canTxBarrierState.epoch;automationResetLocked();}
  else {
    const auto edges=automationEngine.evaluate(automationConfig,automationSensors,now);
    automationActive=edges.active;automationInvalid=edges.invalid;
    automationCamera.maintain(automationSensors,now,cameraAllowed);
    for(unsigned i=0;i<AUTOMATION_RULES;++i){const auto&r=automationConfig.rules[i];const AutomationMask bit=AutomationMask(1u<<i);
      if(!r.enabled)continue;
      if(r.action==AUTOMATION_CAMERA){
        if(edges.invalid&bit)automationCamera.reset();
        if(edges.entered&bit){if(cameraAllowed&&automationCamera.open(automationSensors,now))++automationActions;else ++automationRejected;}
        if(!(edges.active&bit)&&!automationCameraConditionLocked()){if(cameraAllowed&&automationCamera.owned()&&!automationCamera.busy()){
          if(automationCamera.close(automationSensors,now))++automationActions;else {automationCamera.reset();++automationRejected;}
        }}
      }else if((r.trigger?edges.left:edges.entered)&bit)actions[count++]=r.action;
    }
    epoch=automationEpoch;ctx.generation=automationGeneration;ctx.serial=automationStockSerial;
    uint8_t preview[8];tx=cameraAllowed&&automationStockSerial!=automationSentSerial&&automationCamera.prepare(preview,now)!=0;
  }
  portEXIT_CRITICAL(&automationMux);
  for(unsigned i=0;i<count;++i){const bool ok=featureActionEnqueue(actions[i]);portENTER_CRITICAL(&automationMux);if(ok)++automationActions;else ++automationRejected;portEXIT_CRITICAL(&automationMux);}
  if(tx){
    struct can_frame frame={};MCP2515::ERROR error=MCP2515::ERROR_FAIL;
    const bool ok=canTxMcpSendValidated(&frame,epoch,automationValidateCamera,&ctx,error)&&error==MCP2515::ERROR_OK;
    portENTER_CRITICAL(&automationMux);
    if(ctx.generation==automationGeneration&&epoch==automationEpoch){
      automationSentSerial=ctx.serial;
      if(ok)automationCamera.acknowledge(ctx.kind,true,millis());else if(!ctx.canceled){automationCamera.reset();++automationRejected;}
    }
    portEXIT_CRITICAL(&automationMux);
  }
}
