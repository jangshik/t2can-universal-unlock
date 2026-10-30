#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include "../continuous_ap_fsm_pure.h"
#include "../feature_actions.h"
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define pdMS_TO_TICKS(x) (x)
constexpr int pdTRUE=1;
bool labMenuEnabled=true;
bool held=false,saveOk=true,readOk=true,recordPresent=false;
bool autoRecordPresent=false,autoV2Present=false,transportOk=true;uint8_t autoSaved[64]={},autoV2Saved[208]={};
int canTxBarrierMutex=1;uint32_t now=4000,bootTime=0;uint8_t saved[32]={};
uint32_t millis(){return now;}
int xSemaphoreTake(int,int){if(held)return 0;held=true;return 1;}
void xSemaphoreGive(int){assert(held);held=false;}
bool nvsCheckedWriteBlob(const char*,const char*key,const void*p,size_t n){assert(held);if(!saveOk)return false;if(!strcmp(key,"automation_v2")){assert(n==208);memcpy(autoV2Saved,p,n);autoV2Present=true;}else if(!strcmp(key,"automation")){assert(n==64);memcpy(autoSaved,p,n);autoRecordPresent=true;}else{assert(n==32);memcpy(saved,p,n);recordPresent=true;}return true;}
bool nvsCheckedReadBlob(const char*,const char*key,void*p,size_t n,bool& found){const bool a=!strcmp(key,"automation"),v2=!strcmp(key,"automation_v2");found=v2?autoV2Present:(a?autoRecordPresent:recordPresent);if(found)memcpy(p,v2?autoV2Saved:(a?autoSaved:saved),n);return readOk;}
bool vehicleProfileSetupMode=false,vehicleProfileNvsError=false,passive=false;
bool mcpReady=true,twaiReady=true,canTxAdministrativeHold=false;
uint8_t activeVehicleProfile=VEHICLE_MODEL_Y_JUNIPER;
uint8_t activeVehicleTopology=VEHICLE_TOPOLOGY_STANDARD_BODY_CHASSIS;
bool bodySupported=true,macroSupported=true,canUsbTransitionHeld=false;
bool canMaintenanceActive(){return false;}
bool canSteeringRecentTxEcho(uint8_t,const uint8_t*,uint8_t,uint32_t){return false;}
bool activeProfileVehicleBusOnCanA(){return bodySupported;}
bool activeProfileTsl9InputSupported(){return macroSupported;}
bool activeProfileTsl9InputOnBodyCanA(){return bodySupported;}
bool canUsbTxHeld(){return passive;}
bool canUsbPassive(){return passive;}
bool canUsbRequestMode(bool p){passive=p;return true;}
int pedalMapMux=0;struct {bool active=false;}pedalMapSession;
bool apDrive=false;bool apDriveProfileGateOpen(){return apDrive;}
bool continuousApNagBusy(){return false;}
ContApStatus apStatus;ContApStatus continuousApSnapshot(){return apStatus;}
struct {uint32_t epoch=1;}canTxBarrierState;
namespace MCP2515 {enum ERROR{ERROR_OK,ERROR_FAIL};}
struct can_frame{uint16_t can_id=0;uint8_t can_dlc=8,data[8]={};};
std::vector<can_frame> sent;std::vector<uint8_t> actions;
bool featureActionEnqueue(uint8_t a){actions.push_back(a);return true;}
void featureActionClearPending(){actions.clear();}
void(*beforeValidate)()=nullptr;
bool canTxMcpSendValidated(can_frame*f,uint32_t epoch,bool(*validate)(can_frame*,void*),void*c,MCP2515::ERROR&e){
 if(beforeValidate)beforeValidate();
 if(epoch!=canTxBarrierState.epoch||canTxAdministrativeHold||!validate(f,c)||!transportOk)return false;
 sent.push_back(*f);e=MCP2515::ERROR_OK;return true;
}
using esp_err_t=int;
constexpr int ESP_OK=0,CAN_TX_FRESH_BOTH=3;
struct twai_message_t {uint32_t identifier=0;uint8_t data_length_code=8,data[8]={};};
std::vector<twai_message_t> chassisSent;
esp_err_t canTxTwaiTransmitValidated(twai_message_t*f,uint32_t epoch,uint8_t mask,bool(*validate)(twai_message_t*,void*),void*c){
 assert(mask==CAN_TX_FRESH_BOTH);
 if(beforeValidate)beforeValidate();
 if(epoch!=canTxBarrierState.epoch||canTxAdministrativeHold||!validate(f,c))return -1;
 chassisSent.push_back(*f);return ESP_OK;
}
#include "../convenience_runtime.h"
