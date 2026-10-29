#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include "../continuous_ap_fsm_pure.h"
#include "../r79_ap_gate_pure.h"
#include "../feature_actions.h"
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
static uint32_t now=100;
static uint32_t millis(){return now;}
static constexpr uint8_t S3XY_ACTION_BATTERY_PREHEAT=13,S3XY_LOG_INFO=1;
static bool profile=true,saveOk=true,vehicleProfileSetupMode=false,vehicleProfileNvsError=false;
static uint8_t activeVehicleProfile=2,activeVehicleTopology=2;
static bool convenienceActionSupported(uint8_t a){return a==14||profile;}
static std::vector<uint8_t> executed;
static bool convenienceToggleAction(uint8_t a){if(!saveOk)return false;executed.push_back(a);return true;}
static bool s3xyActionSupportedForCurrentProfile(uint8_t a){return a<=13&&profile;}
static void s3xyRunMappedAction(uint8_t,uint8_t a,const char*){executed.push_back(a);}
#define SUPPORT(name) static bool name(){return profile;}
SUPPORT(activeProfileDmsNagSupported) SUPPORT(activeProfileIsaSuppressionSupported)
SUPPORT(activeProfileApDriveProfileSupported) SUPPORT(activeProfileBodyControlsSupported)
SUPPORT(activeProfileTlsscRestoreSupported) SUPPORT(activeProfileBannedCarSupported)
SUPPORT(visionControlSupported) SUPPORT(activeProfileNagSupported)
SUPPORT(activeProfileNagTorqueSupported) SUPPORT(activeProfileNagTsl9Supported)
SUPPORT(activeProfileAdvancedEapSupported) SUPPORT(summonHeartbeatOverrideSupported)
SUPPORT(activeProfileEuUnlockSupported) SUPPORT(activeProfileUlcNoConfirmSupported)
SUPPORT(activeProfileR79Hw3Supported)
static bool activeProfileIsYl(){return false;}
static int continuousApMux=0,nagCfgMux=0,stateMux=0,s3xyMux=0,autoLc293Mux=0,lab3f8Mux=0,researchCaptureMux=0,r79LabMux=0,blinkAMux=0;
static ContApConfig continuousApConfig;
static ContApConfig continuousApControlSnapshot(){return continuousApConfig;}
static bool continuousApControlApply(const ContApConfig&c){if(!saveOk)return false;continuousApConfig=c;return true;}
static bool hold=false;static void setCanTxAdministrativeHold(bool v){hold=v;}
struct NagConfig{bool enabled=false,ignoreApState=false,pauseAtZeroSpeed=false;uint8_t tsl9DowngradeWindow=0;};
static NagConfig nagCfg;
struct Scroll{bool torqueEnabled=false,tsl9Enabled=false;};static Scroll scrollState;
static Scroll nagScrollRuntimeSnapshot(){return scrollState;}
static bool quiesce=true;static bool tsl9InputQuiesceForConfig(){return quiesce;}
static bool nagCfgSaveCandidateWithScroll(const NagConfig&,const Scroll&){return saveOk;}
static void nagScrollRuntimeRestore(const Scroll&s){scrollState=s;}
static void nagTsl9RuntimeReset(){}static void nagCfgCommit(const NagConfig&c){nagCfg=c;}
static void tsl9InputHardResetAfterQuiesce(bool){}
static bool driver=false,isa=false,vision=false;
static bool driverMonitoringControlSnapshot(){return driver;}
static bool driverMonitoringControlApply(bool b){if(!saveOk)return false;driver=b;return true;}
static bool isaSuppressionControlSnapshot(){return isa;}
static bool isaSuppressionControlApply(bool b){if(!saveOk)return false;isa=b;return true;}
static bool visionControlRequestDisabled(){return vision;}
static bool visionControlApplySelection(bool b){if(!saveOk)return false;vision=b;return true;}
static R79ApGateConfigPure r79ApGateConfig{false,0,2};
static bool r79ApControlApply(const R79ApGateConfigPure&c){if(!saveOk)return false;r79ApGateConfig=c;return true;}
static bool r79Hw3Enabled=false;
static bool r79Hw3Apply(bool b){if(!saveOk)return false;r79Hw3Enabled=b;return true;}
struct Summon{bool highway=false,noa=false;};static Summon summon;
static Summon summonCfgRuntimeSnapshot(){return summon;}
static bool summonCfgPersistCandidate(const Summon&){return saveOk;}
static void summonCfgRuntimeRestore(const Summon&s){summon=s;}
static bool s3xyAutoEnabled=false;
struct Preferences{bool begin(const char*,bool){return saveOk;}int putBool(const char*,bool){return saveOk?1:0;}bool getBool(const char*,bool fallback){return !fallback;}void end(){}};
static void s3xyAutoSetEnabled(bool v,bool){s3xyAutoEnabled=v;}
struct Features{struct{bool enabled=false;}autoLaneChange;struct{bool confirmFreeEnabled=false,alcOffHighwayEnabled=false;uint8_t confirmFreeTiming=0;}ulc;};static Features features;
static Features featureConfigRuntimeSnapshot(){return features;}
static bool autoLaneChangeLabCfgPersistCandidate(const Features&f){if(!saveOk)return false;features=f;return true;}
static bool ulcCfgPersistCandidate(const Features&f){if(!saveOk)return false;features=f;return true;}
static bool uiAutoLaneChangeEnabled=false,summonHeartbeatOverrideEnabled=false;
static uint8_t summonHeartbeatOverrideValue=0;
static bool summonHeartbeatOverrideApply(bool b,uint8_t){if(!saveOk)return false;summonHeartbeatOverrideEnabled=b;return true;}
static bool labMenuEnabled=false,preheatMenuEnabled=false;
static bool preheatMenuSetEnabled(bool v){if(!saveOk)return false;preheatMenuEnabled=v;return true;}
static bool featureActionBluetoothToggle(){executed.push_back(42);return true;}
static bool researchCaptureEntries=false,driverMonitorPreRing=false,researchCaptureExporting=false;
static int researchCaptureState=0;
static constexpr int RESEARCH_CAPTURE_CAPTURING=1,RESEARCH_CAPTURE_PAUSED=2;
static bool researchCaptureInit(){return true;}
static bool countryOverrideSetLabEnabledWithBarrier(bool b,bool){if(!saveOk)return false;labMenuEnabled=b;return true;}
static void driverMonitorCaptureInit(){}static void driverMonitorCaptureSuspend(){}static void driverWindowLabResetRuntime(){}
struct FeatureCfg{bool apDrive=false,door=false,restore=false,banned=false;};static FeatureCfg cfg;
static bool bannedCar=false;
static FeatureCfg featureCfgRuntimeSnapshot(){return cfg;}
static bool featureCfgPersistCandidate(const FeatureCfg&){return saveOk;}
static void featureCfgRuntimeRestore(const FeatureCfg&f){cfg=f;bannedCar=f.banned;}
static void s3xyLogPush(uint8_t,const char*){}
struct R79Cfg{bool reinject1=false,reinject2=false;};static R79Cfg r79cfg;
static R79Cfg r79CfgRuntimeSnapshot(){return r79cfg;}
static bool r79CfgPersistCandidate(const R79Cfg&){return saveOk;}
static void r79CfgRuntimeRestore(const R79Cfg&c){r79cfg=c;}
static int r79FixedQuietState=0,r79Mode2DelayedState=0,r79RetryIndex=0,r79RetryOriginKind=0;
static bool r79RetryPending=false;static uint32_t r79RetryDueMs=0,r79RetryGeneration=0,r79Mode1PostMux2Generation=0;
static constexpr int R79LAB_TX_NONE=0,LAB3F8_ALC_FORCE_ON=1,LAB3F8_ALC_STOCK=0,STALK_IDLE=0;
static void canTxCancellationGenerationAdvance(uint32_t*p){++*p;}
static bool ulcNoConfirmEnabled=false;static uint8_t ulcNoConfirmTimingMode=0,lab3f8AlcMode=0;
static int oneShotTurn=0,oneShotUntil=0,oneShotReleaseAt=0,activeTurn=0;
static void autoBlinkerClearPendingLocked(){}
#include "../feature_actions_runtime.h"
int main(){
 continuousApConfig.method=ContApMethod::ScrollDouble;
 assert(featureActionEnqueue(FEATURE_ACTION_MASTER));assert(executed.empty());featureActionService();assert(executed.back()==FEATURE_ACTION_MASTER);
 assert(featureActionEnqueue(FEATURE_ACTION_KICKDOWN));now+=1001;featureActionService();assert(executed.size()==1);
 assert(featureActionEnqueue(FEATURE_ACTION_KICKDOWN));profile=false;featureActionService();assert(!featureActionLastOk&&executed.size()==1);profile=true;
 assert(featureActionExecute(FEATURE_ACTION_CONTINUOUS_AP)&&continuousApConfig.enabled);
 assert(featureActionExecute(FEATURE_ACTION_DRIVER_MONITORING)&&driver);
 saveOk=false;assert(!featureActionExecute(FEATURE_ACTION_DRIVER_MONITORING)&&driver);saveOk=true;
 assert(featureActionExecute(FEATURE_ACTION_NAG)&&nagCfg.enabled&&!hold);
 quiesce=false;assert(!featureActionExecute(FEATURE_ACTION_NAG)&&nagCfg.enabled&&!hold);quiesce=true;
 saveOk=false;assert(!featureActionExecute(FEATURE_ACTION_NAG)&&nagCfg.enabled&&!hold);saveOk=true;
 assert(featureActionExecute(FEATURE_ACTION_CONFIRM_FREE)&&ulcNoConfirmEnabled);
 assert(featureActionExecute(FEATURE_ACTION_R79_REINJECT1)&&r79cfg.reinject1&&!hold);
 assert(featureActionExecute(FEATURE_ACTION_ACTIONS_MENU)&&preheatMenuEnabled);
 assert(featureActionExecute(FEATURE_ACTION_BLUETOOTH)&&executed.back()==42);
 assert(!featureActionSupported(FEATURE_ACTION_AUTO_LANE_CHANGE));assert(featureActionExecute(FEATURE_ACTION_LAB));assert(featureActionSupported(FEATURE_ACTION_AUTO_LANE_CHANGE));
 assert(featureActionExecute(FEATURE_ACTION_AUTO_LANE_CHANGE)&&uiAutoLaneChangeEnabled);
 assert(featureActionEnqueue(FEATURE_ACTION_MASTER));featureActionClearPending();const auto count=executed.size();featureActionService();assert(executed.size()==count);
 for(const auto &action:featureActions){assert(featureActionInfo(action.id));assert(!featureActionCode(action.id)[0]==false);}
}
