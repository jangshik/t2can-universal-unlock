#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Append-only IDs shared by BLE mappings and steering-wheel macros.
// IDs 0..13 remain owned by the original S3XY action registry.
enum FeatureAction : uint8_t {
  FEATURE_ACTION_MASTER = 14, FEATURE_ACTION_KICKDOWN = 15,
  FEATURE_ACTION_EASY_ENTRY = 16, FEATURE_ACTION_REAR_BELT_LEFT = 17,
  FEATURE_ACTION_REAR_BELT_CENTER = 18, FEATURE_ACTION_REAR_BELT_RIGHT = 19,
  FEATURE_ACTION_REVERSE_HAZARD = 20, FEATURE_ACTION_REVERSE_FOG = 21,
  FEATURE_ACTION_MACRO = 22, FEATURE_ACTION_CONTINUOUS_AP = 23,
  FEATURE_ACTION_DRIVER_MONITORING = 24, FEATURE_ACTION_ISA = 25,
  FEATURE_ACTION_AP_DRIVE = 26, FEATURE_ACTION_DOOR_CANCEL = 27,
  FEATURE_ACTION_TLSSC_RESTORE = 28, FEATURE_ACTION_BANNED = 29,
  FEATURE_ACTION_VISION = 30, FEATURE_ACTION_LAB = 31,
  FEATURE_ACTION_NAG = 32, FEATURE_ACTION_RIGHT_SCROLL = 33,
  FEATURE_ACTION_TSL9_PERIODIC = 34, FEATURE_ACTION_R79_AP_GATE = 35,
  FEATURE_ACTION_S3XY_AUTO = 36, FEATURE_ACTION_AUTO_LANE_CHANGE = 37,
  FEATURE_ACTION_SUMMON_HEARTBEAT = 38, FEATURE_ACTION_TLSSC_HIGHWAY = 39,
  FEATURE_ACTION_TLSSC_NOA = 40, FEATURE_ACTION_MIRROR_SYNC = 41,
  FEATURE_ACTION_BLUETOOTH = 42, FEATURE_ACTION_ACTIONS_MENU = 43,
  FEATURE_ACTION_CONFIRM_FREE = 44, FEATURE_ACTION_CONFIRM_PRE_AP = 45,
  FEATURE_ACTION_OFF_HIGHWAY = 46, FEATURE_ACTION_IGNORE_AP = 47,
  FEATURE_ACTION_PAUSE_STOPPED = 48, FEATURE_ACTION_TSL9_WINDOW = 49,
  FEATURE_ACTION_R79_MANUAL = 50, FEATURE_ACTION_R79_HW3 = 51,
  FEATURE_ACTION_R79_REINJECT1 = 52, FEATURE_ACTION_R79_REINJECT2 = 53,
  FEATURE_ACTION_LAST = FEATURE_ACTION_R79_REINJECT2
};
struct FeatureActionInfo { uint8_t id; const char *code; const char *label; };
static constexpr FeatureActionInfo featureActions[] = {
  {14,"master_toggle","CAN Master / Listen Only"},
  {15,"kickdown_toggle","Kickdown"}, {16,"easy_entry_toggle","Easy Entry"},
  {17,"rear_belt_left_toggle","Rear Belt Left"}, {18,"rear_belt_center_toggle","Rear Belt Center"},
  {19,"rear_belt_right_toggle","Rear Belt Right"}, {20,"reverse_hazard_toggle","Reverse Hazard"},
  {21,"reverse_fog_toggle","Reverse Rear Fog"}, {22,"macro_toggle","Steering Macros"},
  {23,"continuous_ap_toggle","Continuous AP"},
  {24,"driver_monitoring_toggle","Disable Driver Monitoring"}, {25,"isa_toggle","ISA Suppression"},
  {26,"ap_drive_toggle","AP Drive Profile"}, {27,"door_cancel_toggle","Door Lane Change Cancel"},
  {28,"tlssc_restore_toggle","TLSSC Restore"}, {29,"banned_toggle","Banned Car Mode"},
  {30,"vision_toggle","Visual Speed Control"}, {31,"lab_toggle","LAB"},
  {32,"nag_toggle","NAG"}, {33,"right_scroll_toggle","AP Right Scroll"},
  {34,"tsl9_periodic_toggle","Periodic Right Scroll"}, {35,"r79_ap_gate_toggle","R79 AP Gate"},
  {36,"s3xy_auto_toggle","S3XY Auto Connect"}, {37,"auto_lane_change_toggle","Auto Lane Change"},
  {38,"summon_heartbeat_toggle","Summon Heartbeat"}, {39,"tlssc_highway_toggle","TLSSC Highway Block"},
  {40,"tlssc_noa_toggle","TLSSC NOA Block"},
  {41,"mirror_sync_toggle","Easy Entry Mirror Sync"},
  {42,"bluetooth_toggle","Bluetooth Master"},
  {43,"actions_menu_toggle","Actions Menu"},
  {44,"confirm_free_toggle","Confirm-Free Lane Change"},
  {45,"confirm_pre_ap_toggle","Confirm-Free Before AP"},
  {46,"off_highway_toggle","Off-Highway ALC"},
  {47,"nag_ignore_ap_toggle","NAG Ignore AP State"},
  {48,"nag_pause_stopped_toggle","NAG Pause at Zero Speed"},
  {49,"tsl9_window_toggle","TSL9 Downgrade Window"},
  {50,"r79_manual_toggle","R79 Allow Manual Driving"},
  {51,"r79_hw3_toggle","R79 HW3"},
  {52,"r79_reinject1_toggle","R79 Mode 1 Reinjection"},
  {53,"r79_reinject2_toggle","R79 Mode 2 Reinjection"}
};
static inline const FeatureActionInfo *featureActionInfo(uint8_t action) {
  for (const auto &item : featureActions) if (item.id == action) return &item;
  return nullptr;
}
static inline const char *featureActionCode(uint8_t action) {
  const auto *info = featureActionInfo(action); return info ? info->code : "none";
}
static inline const char *featureActionLabel(uint8_t action) {
  const auto *info = featureActionInfo(action); return info ? info->label : "None";
}
static inline bool featureActionParse(const char *text, uint8_t &action) {
  if (!text) return false;
  for (const auto &item : featureActions) {
    if (!strcmp(text,item.code) || !strcmp(text,item.label)) { action=item.id; return true; }
  }
  return false;
}
// Queue operations require the caller's lock. Expired commands are discarded,
// so delayed BLE/config work cannot activate a feature after the gesture context.
class FeatureActionQueue {
 public:
  static constexpr uint8_t capacity = 16;
  bool push(uint8_t action, uint32_t now) {
    if (!action || action > FEATURE_ACTION_LAST || count_ == capacity) return false;
    entries_[(head_+count_)%capacity]={action,now}; ++count_; return true;
  }
  bool pop(uint32_t now,uint8_t &action) {
    while(count_) {
      const Entry entry=entries_[head_]; head_=(head_+1)%capacity; --count_;
      if (uint32_t(now-entry.at)<=1000u) { action=entry.action; return true; }
    }
    return false;
  }
  void clear() { head_=count_=0; }
 private:
  struct Entry {uint8_t action;uint32_t at;};
  Entry entries_[capacity]{}; uint8_t head_=0,count_=0;
};
