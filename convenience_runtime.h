#pragma once
#include "convenience_pure.h"
#include "macro_gesture_pure.h"
#include "feature_actions.h"

// One configuration owner (webTask, also servicing queued BLE/macro actions).
// RX state is protected separately; never hold a spinlock while writing NVS/TX.
static portMUX_TYPE convenienceMux = portMUX_INITIALIZER_UNLOCKED;
static ConvenienceConfig convenienceConfig;
static MacroGestureConfig convenienceMacroConfig;
static ConvenienceEngine convenienceEngine;
static MacroGestureEngine convenienceMacros;
static volatile uint32_t convenienceGeneration = 1;
static uint32_t convenienceEpoch = 0, convenienceTxOk = 0, convenienceTxFail = 0;
static bool convenienceNvsError = false;

// Easy Entry is user-controlled and defaults OFF.
static bool convenienceBodySupported() {
  return !vehicleProfileSetupMode && !vehicleProfileNvsError &&
      activeVehicleProfile==VEHICLE_MODEL_Y_JUNIPER && activeProfileVehicleBusOnCanA();
}
static bool convenienceKickdownSupported() {
  return !vehicleProfileSetupMode && !vehicleProfileNvsError &&
      activeVehicleProfile==VEHICLE_MODEL_Y_JUNIPER &&
      (convenienceBodySupported()||activeVehicleTopology==VEHICLE_TOPOLOGY_STANDARD_PARTY_CHASSIS);
}
static bool convenienceMacroSupported() {
  return convenienceBodySupported() && activeProfileTsl9InputSupported();
}
static bool convenienceActionSupported(uint8_t action) {
  if (action == FEATURE_ACTION_MASTER) return !vehicleProfileSetupMode && !vehicleProfileNvsError;
  if (action == FEATURE_ACTION_KICKDOWN) return convenienceKickdownSupported();
  if (action == FEATURE_ACTION_MACRO) return convenienceMacroSupported();
  if (action == FEATURE_ACTION_EASY_ENTRY || action == FEATURE_ACTION_MIRROR_SYNC)
    return convenienceBodySupported();
  return action >= FEATURE_ACTION_KICKDOWN && action <= FEATURE_ACTION_REVERSE_FOG && convenienceBodySupported();
}
static void convenienceSnapshot(ConvenienceConfig &c, MacroGestureConfig &m) {
  portENTER_CRITICAL(&convenienceMux); c=convenienceConfig; m=convenienceMacroConfig; portEXIT_CRITICAL(&convenienceMux);
}
// Explicit versioned wire format: no compiler padding or bool representation
// in NVS. A single committed blob prevents partially saved gesture mappings.
static void convenienceEncode(const ConvenienceConfig &c,const MacroGestureConfig &m,uint8_t b[32]) {
  memset(b,0,32); b[0]=1;
  const bool flags[]={c.kickdown,c.easyEntry,c.mirrorSync,c.rearBeltLeft,c.rearBeltCenter,c.rearBeltRight,c.reverseHazard,c.reverseFog};
  for(unsigned n=0;n<8;++n)if(flags[n])b[1]|=1u<<n;
  b[2]=c.kickOnPct;b[3]=c.kickOffPct;b[4]=c.kickReleaseMode;b[5]=m.enabled;
  const uint16_t timings[]={m.debounceMs,m.clickGapMs,m.longPressMs,m.freshnessMs};
  for(unsigned n=0;n<4;++n){b[6+2*n]=timings[n]&255;b[7+2*n]=timings[n]>>8;}
  for(unsigned n=0;n<3;++n){b[14+3*n]=m.slots[n].button;b[15+3*n]=uint8_t(m.slots[n].gesture);b[16+3*n]=m.slots[n].action;}
}
static bool convenienceDecode(const uint8_t b[32],ConvenienceConfig &c,MacroGestureConfig &m) {
  if(b[0]!=1||b[5]>1)return false;
  for(unsigned n=23;n<32;++n)if(b[n])return false;
  bool* flags[]={&c.kickdown,&c.easyEntry,&c.mirrorSync,&c.rearBeltLeft,&c.rearBeltCenter,&c.rearBeltRight,&c.reverseHazard,&c.reverseFog};
  for(unsigned n=0;n<8;++n)*flags[n]=(b[1]&(1u<<n))!=0;
  c.kickOnPct=b[2];c.kickOffPct=b[3];c.kickReleaseMode=b[4];m.enabled=b[5];
  uint16_t* timings[]={&m.debounceMs,&m.clickGapMs,&m.longPressMs,&m.freshnessMs};
  for(unsigned n=0;n<4;++n)*timings[n]=uint16_t(b[6+2*n])|(uint16_t(b[7+2*n])<<8);
  for(unsigned n=0;n<3;++n){m.slots[n].button=b[14+3*n];m.slots[n].gesture=MacroGesture(b[15+3*n]);m.slots[n].action=b[16+3*n];if(m.slots[n].action>FEATURE_ACTION_LAST)return false;}
  return convenienceConfigValidPure(c)&&macroGestureConfigValidPure(m);
}
static void convenienceResetUnderTxBarrier() {
  portENTER_CRITICAL(&convenienceMux);
  ++convenienceGeneration;convenienceEpoch=canTxBarrierState.epoch;
  convenienceEngine.reset(convenienceEpoch);convenienceMacros.reset();
  portEXIT_CRITICAL(&convenienceMux);
  featureActionClearPending();
}
static void convenienceLoad() {
  ConvenienceConfig c; MacroGestureConfig m;uint8_t bytes[32]={};bool found=false;
  convenienceNvsError=!nvsCheckedReadBlob("features","convenience",bytes,sizeof(bytes),found);
  if(!convenienceNvsError&&found)convenienceNvsError=!convenienceDecode(bytes,c,m);
  if(convenienceNvsError){c=ConvenienceConfig{};m=MacroGestureConfig{};}
  portENTER_CRITICAL(&convenienceMux);
  convenienceConfig=c;convenienceMacroConfig=m;convenienceMacros.configure(m);
  portEXIT_CRITICAL(&convenienceMux);
}
static bool convenienceApply(const ConvenienceConfig &c,const MacroGestureConfig &m) {
  if(!convenienceConfigValidPure(c)||!macroGestureConfigValidPure(m))return false;
  if(!canTxBarrierMutex||xSemaphoreTake(canTxBarrierMutex,pdMS_TO_TICKS(500))!=pdTRUE)return false;
  uint8_t bytes[32];convenienceEncode(c,m,bytes);
  const bool ok=nvsCheckedWriteBlob("features","convenience",bytes,sizeof(bytes));
  if(ok){
    portENTER_CRITICAL(&convenienceMux);
    convenienceConfig=c;convenienceMacroConfig=m;convenienceNvsError=false;
    ++convenienceGeneration;convenienceMacros.configure(m);
    portEXIT_CRITICAL(&convenienceMux);
    featureActionClearPending();
  }
  xSemaphoreGive(canTxBarrierMutex);return ok;
}
static bool convenienceToggleAction(uint8_t action) {
  if(!convenienceActionSupported(action))return false;
  if(action==FEATURE_ACTION_MASTER)return canUsbRequestMode(!canUsbPassive());
  ConvenienceConfig c;MacroGestureConfig m;convenienceSnapshot(c,m);
  switch(action){
    case FEATURE_ACTION_KICKDOWN:c.kickdown=!c.kickdown;break;
    case FEATURE_ACTION_EASY_ENTRY:c.easyEntry=!c.easyEntry;break;
    case FEATURE_ACTION_MIRROR_SYNC:c.mirrorSync=!c.mirrorSync;break;
    case FEATURE_ACTION_REAR_BELT_LEFT:c.rearBeltLeft=!c.rearBeltLeft;break;
    case FEATURE_ACTION_REAR_BELT_CENTER:c.rearBeltCenter=!c.rearBeltCenter;break;
    case FEATURE_ACTION_REAR_BELT_RIGHT:c.rearBeltRight=!c.rearBeltRight;break;
    case FEATURE_ACTION_REVERSE_HAZARD:c.reverseHazard=!c.reverseHazard;break;
    case FEATURE_ACTION_REVERSE_FOG:c.reverseFog=!c.reverseFog;break;
    case FEATURE_ACTION_MACRO:m.enabled=!m.enabled;break;
    default:return false;
  }
  return convenienceApply(c,m);
}
static bool conveniencePedalAvailable() {
  portENTER_CRITICAL(&pedalMapMux);const bool manual=pedalMapSession.active;portEXIT_CRITICAL(&pedalMapMux);
  return !manual&&!apDriveProfileGateOpen();
}
struct ConvenienceTxContext {uint32_t generation,rxMs;};
static bool convenienceValidateTx(struct can_frame *frame,void *context) {
  const auto &c=*static_cast<ConvenienceTxContext*>(context);
  return convenienceBodySupported()&&!canUsbTxHeld()&&mcpReady&&twaiReady&&
    c.generation==convenienceGeneration&&uint32_t(millis()-c.rxMs)<=50&&
    (frame->can_id!=0x334||conveniencePedalAvailable());
}
static bool convenienceValidateChassisTx(twai_message_t *frame,void *context) {
  const auto &c=*static_cast<ConvenienceTxContext*>(context);
  return frame->identifier==0x334&&convenienceKickdownSupported()&&
    activeVehicleTopology==VEHICLE_TOPOLOGY_STANDARD_PARTY_CHASSIS&&
    !canUsbTxHeld()&&mcpReady&&twaiReady&&c.generation==convenienceGeneration&&
    uint32_t(millis()-c.rxMs)<=50&&conveniencePedalAvailable();
}
// Reject actuator-generated 0x3C2 traffic before gesture recognition, even when
// another feature owns the steering controls. Physical presses always continue
// through the repository's original arbitration path.
static bool convenienceSteeringBusy() {
  const auto ap=continuousApSnapshot();
  return ap.releaseOwed||ap.state==ContApState::SendGesture||continuousApNagBusy();
}
static void convenienceObserve(uint8_t bus,uint16_t id,const uint8_t *data,uint8_t dlc,uint32_t epoch,uint32_t rxMs) {
  if(!data||dlc>8)return;
  const bool body=bus==0&&convenienceBodySupported();
  const bool macroRoute=convenienceMacroSupported()&&bus==(activeProfileTsl9InputOnBodyCanA()?0:1);
  const bool chassisKick=bus==1&&convenienceKickdownSupported()&&
    activeVehicleTopology==VEHICLE_TOPOLOGY_STANDARD_PARTY_CHASSIS&&
    (id==0x118||id==0x39d||id==0x334);
  if(!body&&!macroRoute&&!chassisKick)return;
  const bool allowed=!canUsbTxHeld()&&!canTxAdministrativeHold&&mcpReady&&twaiReady;
  const bool steeringBusy=convenienceSteeringBusy();
  const bool macroAllowed=!canMaintenanceActive()&&!__atomic_load_n(&canUsbTransitionHeld,__ATOMIC_ACQUIRE);
  const bool echo=id==0x3c2&&canSteeringRecentTxEcho(bus,data,dlc,rxMs);
  const bool pedalAvailable=conveniencePedalAvailable();
  ConvenienceOutput out;uint32_t generation;
  portENTER_CRITICAL(&convenienceMux);
  if(convenienceEpoch!=epoch){convenienceEpoch=epoch;convenienceEngine.reset(epoch);convenienceMacros.reset();}
  generation=convenienceGeneration;
  if(macroRoute&&id==0x3c2){
    if(!macroAllowed||steeringBusy||echo)convenienceMacros.reset();
    else convenienceMacros.observe(id,dlc,data,rxMs);
  }
  if((body||chassisKick)&&!echo){
    auto cfg=convenienceConfig;
    if(chassisKick){const auto saved=cfg;cfg={};cfg.kickdown=saved.kickdown;
      cfg.kickOnPct=saved.kickOnPct;cfg.kickOffPct=saved.kickOffPct;cfg.kickReleaseMode=saved.kickReleaseMode;}
    cfg.kickdown=cfg.kickdown&&pedalAvailable;
    out=convenienceEngine.process(cfg,id,data,dlc,rxMs,epoch,allowed&&uint32_t(rxMs-bootTime)>=3000);
  }
  portEXIT_CRITICAL(&convenienceMux);
  for(uint8_t n=0;n<out.count;++n){
    const auto &f=out.frames[n];struct can_frame tx={};tx.can_id=f.id;tx.can_dlc=f.dlc;memcpy(tx.data,f.data,8);
    ConvenienceTxContext ctx{generation,rxMs};MCP2515::ERROR error=MCP2515::ERROR_FAIL;
    bool sent=false;
    if(chassisKick){
      twai_message_t chassis={};chassis.identifier=f.id;chassis.data_length_code=f.dlc;memcpy(chassis.data,f.data,8);
      sent=canTxTwaiTransmitValidated(&chassis,epoch,CAN_TX_FRESH_BOTH,convenienceValidateChassisTx,&ctx)==ESP_OK;
    }else sent=canTxMcpSendValidated(&tx,epoch,convenienceValidateTx,&ctx,error)&&error==MCP2515::ERROR_OK;
    portENTER_CRITICAL(&convenienceMux);
    if(generation==convenienceGeneration&&epoch==convenienceEpoch)convenienceEngine.acknowledge(f,sent);
    if(sent)++convenienceTxOk;else ++convenienceTxFail;
    portEXIT_CRITICAL(&convenienceMux);
  }
}
static void convenienceService() {
  uint8_t actions[3]={};unsigned count=0;
  const bool allowed=convenienceMacroSupported()&&!canMaintenanceActive()&&!__atomic_load_n(&canUsbTransitionHeld,__ATOMIC_ACQUIRE);
  portENTER_CRITICAL(&convenienceMux);
  if(!allowed)convenienceMacros.reset();
  else {convenienceMacros.poll((uint32_t)millis());while(count<3&&convenienceMacros.popAction(actions[count]))++count;}
  portEXIT_CRITICAL(&convenienceMux);
  for(unsigned n=0;n<count;++n)featureActionEnqueue(actions[n]);
}

// LAB commands are single-shot and never saved or replayed. Keep automatic
// Easy Entry OFF during manual testing so it cannot compete with a test.
static uint32_t convenienceWindowTestCount=0;
static ConvenienceFrame convenienceWindowLastTest;
struct ConvenienceWindowTestContext {ConvenienceTxContext tx;uint8_t type,percent,corner;};
static bool convenienceValidateWindowTest(struct can_frame *frame,void *context) {
  auto &c=*static_cast<ConvenienceWindowTestContext*>(context);
  if(!labMenuEnabled||!convenienceValidateTx(frame,&c.tx))return false;
  portENTER_CRITICAL(&convenienceMux);
  ConvenienceFrame latest;
  const bool ready=!convenienceConfig.easyEntry&&convenienceEngine.prepareWindowTest(c.type,c.percent,c.corner,millis(),latest);
  if(ready)memcpy(frame->data,latest.data,8);
  portEXIT_CRITICAL(&convenienceMux);
  return ready;
}
static bool convenienceWindowTest(uint8_t type,uint8_t percent,uint8_t corner) {
  if(!labMenuEnabled||!convenienceBodySupported()||canUsbTxHeld())return false;
  ConvenienceFrame f;const uint32_t now=millis();uint32_t generation,epoch;
  portENTER_CRITICAL(&convenienceMux);
  generation=convenienceGeneration;epoch=convenienceEpoch;
  const bool ready=!convenienceConfig.easyEntry&&convenienceEngine.prepareWindowTest(type,percent,corner,now,f);
  portEXIT_CRITICAL(&convenienceMux);
  if(!ready)return false;
  struct can_frame tx={};tx.can_id=f.id;tx.can_dlc=f.dlc;memcpy(tx.data,f.data,8);
  ConvenienceWindowTestContext ctx{{generation,now},type,percent,corner};MCP2515::ERROR error=MCP2515::ERROR_FAIL;
  const bool sent=canTxMcpSendValidated(&tx,epoch,convenienceValidateWindowTest,&ctx,error)&&error==MCP2515::ERROR_OK;
  if(sent){convenienceWindowLastTest=f;memcpy(convenienceWindowLastTest.data,tx.data,8);++convenienceWindowTestCount;}
  return sent;
}
