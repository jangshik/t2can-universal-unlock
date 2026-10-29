#pragma once
#include "feature_actions.h"

// Included after vehicle, Continuous AP and convenience runtime definitions.
// The queue is the only state touched by BLE / CAN RX producers. NVS writes
// execute on the web/main task, serialized with WebServer configuration calls.
static bool featureActionBluetoothToggle();
static portMUX_TYPE featureActionMux = portMUX_INITIALIZER_UNLOCKED;
static FeatureActionQueue featureActionQueue;
static uint8_t featureActionLast = 0;
static bool featureActionLastOk = true;
static uint32_t featureActionRejected = 0;

static bool featureActionSupported(uint8_t action) {
  if ((action >= FEATURE_ACTION_MASTER && action <= FEATURE_ACTION_MACRO) || action == FEATURE_ACTION_MIRROR_SYNC)
    return convenienceActionSupported(action);
  if (!featureActionInfo(action))
    return action <= S3XY_ACTION_BATTERY_PREHEAT && s3xyActionSupportedForCurrentProfile(action);
  if (vehicleProfileSetupMode || vehicleProfileNvsError) return false;
  switch(action) {
    case FEATURE_ACTION_CONTINUOUS_AP: {
      portENTER_CRITICAL(&continuousApMux); const auto cfg=continuousApConfig; portEXIT_CRITICAL(&continuousApMux);
      return continuousApGestureRoutePure(activeVehicleProfile,activeVehicleTopology,cfg.method)!=ContApRoute::None;
    }
    case FEATURE_ACTION_DRIVER_MONITORING:return activeProfileDmsNagSupported();
    case FEATURE_ACTION_ISA:return activeProfileIsaSuppressionSupported();
    case FEATURE_ACTION_AP_DRIVE:return activeProfileApDriveProfileSupported();
    case FEATURE_ACTION_DOOR_CANCEL:return activeProfileBodyControlsSupported() || activeProfileIsYl();
    case FEATURE_ACTION_TLSSC_RESTORE:return activeProfileTlsscRestoreSupported() && activeProfileBannedCarSupported() && bannedCar;
    case FEATURE_ACTION_BANNED:return activeProfileBannedCarSupported();
    case FEATURE_ACTION_VISION:return visionControlSupported();
    case FEATURE_ACTION_NAG:case FEATURE_ACTION_IGNORE_AP:case FEATURE_ACTION_PAUSE_STOPPED:return activeProfileNagSupported();
    case FEATURE_ACTION_TSL9_WINDOW:return activeProfileNagTsl9Supported();
    case FEATURE_ACTION_CONFIRM_FREE:case FEATURE_ACTION_CONFIRM_PRE_AP:return activeProfileUlcNoConfirmSupported();
    case FEATURE_ACTION_OFF_HIGHWAY:return activeProfileAdvancedEapSupported();
    case FEATURE_ACTION_R79_HW3:return activeProfileR79Hw3Supported();
    case FEATURE_ACTION_R79_MANUAL:case FEATURE_ACTION_R79_REINJECT1:case FEATURE_ACTION_R79_REINJECT2:return activeProfileEuUnlockSupported();
    case FEATURE_ACTION_RIGHT_SCROLL:return activeProfileNagTorqueSupported();
    case FEATURE_ACTION_TSL9_PERIODIC:return activeProfileNagTsl9Supported();
    case FEATURE_ACTION_AUTO_LANE_CHANGE:return labMenuEnabled && activeProfileAdvancedEapSupported();
    case FEATURE_ACTION_SUMMON_HEARTBEAT:return labMenuEnabled && summonHeartbeatOverrideSupported();
    case FEATURE_ACTION_R79_AP_GATE:return labMenuEnabled && activeProfileEuUnlockSupported();
    default:return true;
  }
}
static bool featureActionEnqueue(uint8_t action) {
  if (!action || !featureActionSupported(action)) return false;
  portENTER_CRITICAL(&featureActionMux);
  const bool ok=featureActionQueue.push(action,(uint32_t)millis());
  if(!ok)++featureActionRejected;
  portEXIT_CRITICAL(&featureActionMux);return ok;
}
static void featureActionClearPending() {
  portENTER_CRITICAL(&featureActionMux);featureActionQueue.clear();portEXIT_CRITICAL(&featureActionMux);
}
static bool featureActionApplyNag(uint8_t action) {
  portENTER_CRITICAL(&nagCfgMux);auto next=nagCfg;portEXIT_CRITICAL(&nagCfgMux);
  auto scroll=nagScrollRuntimeSnapshot();
  if(action==FEATURE_ACTION_NAG)next.enabled=!next.enabled;
  else if(action==FEATURE_ACTION_RIGHT_SCROLL)scroll.torqueEnabled=!scroll.torqueEnabled;
  else if(action==FEATURE_ACTION_TSL9_PERIODIC)scroll.tsl9Enabled=!scroll.tsl9Enabled;
  else if(action==FEATURE_ACTION_IGNORE_AP)next.ignoreApState=!next.ignoreApState;
  else if(action==FEATURE_ACTION_PAUSE_STOPPED)next.pauseAtZeroSpeed=!next.pauseAtZeroSpeed;
  else if(action==FEATURE_ACTION_TSL9_WINDOW)next.tsl9DowngradeWindow=next.tsl9DowngradeWindow?0:1;
  else return false;
  if(!tsl9InputQuiesceForConfig())return false;
  setCanTxAdministrativeHold(true);
  if(!nagCfgSaveCandidateWithScroll(next,scroll)){setCanTxAdministrativeHold(false);return false;}
  nagScrollRuntimeRestore(scroll);nagTsl9RuntimeReset();nagCfgCommit(next);
  tsl9InputHardResetAfterQuiesce(false);setCanTxAdministrativeHold(false);return true;
}
static bool featureActionExecute(uint8_t action) {
  // Recheck: a command queued under another profile or LAB state is stale.
  if(!featureActionSupported(action))return false;
  if((action>=FEATURE_ACTION_MASTER && action<=FEATURE_ACTION_MACRO)||action==FEATURE_ACTION_MIRROR_SYNC)return convenienceToggleAction(action);
  if(action<=S3XY_ACTION_BATTERY_PREHEAT){s3xyRunMappedAction(0,action,"Steering macro");return true;}
  switch(action) {
    case FEATURE_ACTION_BLUETOOTH:return featureActionBluetoothToggle();
    case FEATURE_ACTION_ACTIONS_MENU:return preheatMenuSetEnabled(!preheatMenuEnabled);
    case FEATURE_ACTION_CONTINUOUS_AP:{
      portENTER_CRITICAL(&continuousApMux);auto next=continuousApConfig;portEXIT_CRITICAL(&continuousApMux);
      next.enabled=!next.enabled;return continuousApControlApply(next);
    }
    case FEATURE_ACTION_DRIVER_MONITORING:return driverMonitoringControlApply(!driverMonitoringControlSnapshot());
    case FEATURE_ACTION_ISA:return isaSuppressionControlApply(!isaSuppressionControlSnapshot());
    case FEATURE_ACTION_VISION:return visionControlApplySelection(!visionControlRequestDisabled());
    case FEATURE_ACTION_NAG:case FEATURE_ACTION_RIGHT_SCROLL:case FEATURE_ACTION_TSL9_PERIODIC:
    case FEATURE_ACTION_IGNORE_AP:case FEATURE_ACTION_PAUSE_STOPPED:case FEATURE_ACTION_TSL9_WINDOW:return featureActionApplyNag(action);
    case FEATURE_ACTION_R79_HW3:return r79Hw3Apply(!r79Hw3Enabled);
    case FEATURE_ACTION_R79_MANUAL:{
      portENTER_CRITICAL(&stateMux);auto next=r79ApGateConfig;portEXIT_CRITICAL(&stateMux);
      next.allowManualDriving=!next.allowManualDriving;return r79ApControlApply(next);
    }
    case FEATURE_ACTION_R79_REINJECT1:case FEATURE_ACTION_R79_REINJECT2:{
      auto next=r79CfgRuntimeSnapshot();
      if(action==FEATURE_ACTION_R79_REINJECT1)next.reinject1=!next.reinject1;else next.reinject2=!next.reinject2;
      setCanTxAdministrativeHold(true);
      const bool ok=r79CfgPersistCandidate(next);
      if(ok){r79CfgRuntimeRestore(next);
        portENTER_CRITICAL(&r79LabMux);r79FixedQuietState={};r79Mode2DelayedState={};r79RetryPending=false;r79RetryIndex=0;r79RetryOriginKind=R79LAB_TX_NONE;r79RetryDueMs=0;r79RetryGeneration=0;portEXIT_CRITICAL(&r79LabMux);
        canTxCancellationGenerationAdvance(&r79Mode1PostMux2Generation);
      }
      setCanTxAdministrativeHold(false);return ok;
    }
    case FEATURE_ACTION_CONFIRM_FREE:case FEATURE_ACTION_CONFIRM_PRE_AP:case FEATURE_ACTION_OFF_HIGHWAY:{
      auto next=featureConfigRuntimeSnapshot();
      if(action==FEATURE_ACTION_CONFIRM_FREE)next.ulc.confirmFreeEnabled=!next.ulc.confirmFreeEnabled;
      else if(action==FEATURE_ACTION_CONFIRM_PRE_AP)next.ulc.confirmFreeTiming=next.ulc.confirmFreeTiming?0:1;
      else next.ulc.alcOffHighwayEnabled=!next.ulc.alcOffHighwayEnabled;
      if(!ulcCfgPersistCandidate(next))return false;
      portENTER_CRITICAL(&lab3f8Mux);
      ulcNoConfirmEnabled=next.ulc.confirmFreeEnabled;ulcNoConfirmTimingMode=next.ulc.confirmFreeTiming;
      lab3f8AlcMode=next.ulc.alcOffHighwayEnabled?LAB3F8_ALC_FORCE_ON:LAB3F8_ALC_STOCK;
      portEXIT_CRITICAL(&lab3f8Mux);
      if(next.ulc.confirmFreeEnabled){portENTER_CRITICAL(&blinkAMux);autoBlinkerClearPendingLocked();oneShotTurn=STALK_IDLE;oneShotUntil=0;oneShotReleaseAt=0;activeTurn=STALK_IDLE;portEXIT_CRITICAL(&blinkAMux);}
      return true;
    }
    case FEATURE_ACTION_R79_AP_GATE:{
      portENTER_CRITICAL(&stateMux);auto next=r79ApGateConfig;portEXIT_CRITICAL(&stateMux);
      next.enabled=!next.enabled;return r79ApControlApply(next);
    }
    case FEATURE_ACTION_TLSSC_HIGHWAY:case FEATURE_ACTION_TLSSC_NOA:{
      auto next=summonCfgRuntimeSnapshot();
      if(action==FEATURE_ACTION_TLSSC_HIGHWAY)next.highway=!next.highway;else next.noa=!next.noa;
      if(!summonCfgPersistCandidate(next))return false;
      summonCfgRuntimeRestore(next);return true;
    }
    case FEATURE_ACTION_S3XY_AUTO:{
      portENTER_CRITICAL(&s3xyMux);const bool enabled=!s3xyAutoEnabled;portEXIT_CRITICAL(&s3xyMux);
      Preferences p;if(!p.begin("s3xy",false))return false;
      const bool saved=p.putBool("auto",enabled)==1u && p.getBool("auto",!enabled)==enabled;p.end();
      if(saved)s3xyAutoSetEnabled(enabled,false);
      return saved;
    }
    case FEATURE_ACTION_AUTO_LANE_CHANGE:{
      auto next=featureConfigRuntimeSnapshot();next.autoLaneChange.enabled=!next.autoLaneChange.enabled;
      if(!autoLaneChangeLabCfgPersistCandidate(next))return false;
      portENTER_CRITICAL(&autoLc293Mux);uiAutoLaneChangeEnabled=next.autoLaneChange.enabled;portEXIT_CRITICAL(&autoLc293Mux);return true;
    }
    case FEATURE_ACTION_SUMMON_HEARTBEAT:{
      portENTER_CRITICAL(&lab3f8Mux);const bool enabled=!summonHeartbeatOverrideEnabled;const uint8_t value=summonHeartbeatOverrideValue;portEXIT_CRITICAL(&lab3f8Mux);
      return summonHeartbeatOverrideApply(enabled,value);
    }
    case FEATURE_ACTION_LAB:{
      const bool enabled=!labMenuEnabled;
      if(enabled&&!researchCaptureEntries&&!researchCaptureInit())return false;
      if(!countryOverrideSetLabEnabledWithBarrier(enabled,true))return false;
      if(enabled&&!driverMonitorPreRing)driverMonitorCaptureInit();
      if(!enabled){
        portENTER_CRITICAL(&researchCaptureMux);
        if(!researchCaptureExporting&&researchCaptureState==RESEARCH_CAPTURE_CAPTURING)researchCaptureState=RESEARCH_CAPTURE_PAUSED;
        portEXIT_CRITICAL(&researchCaptureMux);driverMonitorCaptureSuspend();driverWindowLabResetRuntime();
      }
      return true;
    }
    default:{
      auto next=featureCfgRuntimeSnapshot();
      if(action==FEATURE_ACTION_AP_DRIVE)next.apDrive=!next.apDrive;
      else if(action==FEATURE_ACTION_DOOR_CANCEL)next.door=!next.door;
      else if(action==FEATURE_ACTION_TLSSC_RESTORE)next.restore=!next.restore;
      else if(action==FEATURE_ACTION_BANNED){next.banned=!next.banned;if(!next.banned)next.restore=false;}
      else return false;
      if(action==FEATURE_ACTION_BANNED)setCanTxAdministrativeHold(true);
      const bool ok=featureCfgPersistCandidate(next);
      if(ok)featureCfgRuntimeRestore(next);
      if(action==FEATURE_ACTION_BANNED)setCanTxAdministrativeHold(false);
      return ok;
    }
  }
}
static void featureActionService() {
  uint8_t action=0;
  portENTER_CRITICAL(&featureActionMux);const bool pending=featureActionQueue.pop((uint32_t)millis(),action);portEXIT_CRITICAL(&featureActionMux);
  if(!pending)return;
  featureActionLast=action;featureActionLastOk=featureActionExecute(action);
  if(!featureActionLastOk)s3xyLogPush(S3XY_LOG_INFO,"Feature action rejected or settings save failed");
}
