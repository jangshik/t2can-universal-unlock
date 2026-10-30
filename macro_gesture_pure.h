#pragma once
#include <stdint.h>

// Observe accepted physical Body CAN frames only. Call reset() whenever the
// profile, CAN epoch or TX ownership changes; never feed injected TX echoes.
enum class MacroGesture : uint8_t { Double = 2, Triple = 3, Long = 4 };
enum class MacroButton : uint8_t {
  LeftWheel, RightWheel, LeftTiltRight, RightTiltLeft, RightTiltRight,
  HighBeam, Wiper, Voice, Camera, Count
};
struct MacroGestureSlot {
  uint8_t button = 0;
  MacroGesture gesture = MacroGesture::Double;
  uint8_t action = 0;
};
struct MacroGestureConfig {
  bool enabled = false;
  uint16_t debounceMs = 30;
  uint16_t clickGapMs = 350;
  uint16_t longPressMs = 800;
  uint16_t freshnessMs = 250;
  MacroGestureSlot slots[3]{};
};
static inline bool macroGestureConfigValidPure(const MacroGestureConfig &c) {
  if (c.debounceMs < 5 || c.debounceMs > 100 ||
      c.clickGapMs < 100 || c.clickGapMs > 1500 ||
      c.longPressMs < 200 || c.longPressMs > 5000 ||
      c.freshnessMs < 100 || c.freshnessMs > 1000 ||
      c.debounceMs >= c.freshnessMs) return false;
  for (const auto &s : c.slots) {
    if (s.button >= uint8_t(MacroButton::Count) ||
        (s.gesture != MacroGesture::Double && s.gesture != MacroGesture::Triple &&
         s.gesture != MacroGesture::Long)) return false;
  }
  return true;
}

class MacroGestureEngine {
 public:
  bool configure(const MacroGestureConfig &c) {
    if (!macroGestureConfigValidPure(c)) return false;
    config_ = c;
    reset();
    return true;
  }
  const MacroGestureConfig &config() const { return config_; }
  void reset() {
    for (auto &b : buttons_) b = ButtonState{};
    seen_ = false;
    lastRx_ = 0;
    head_ = count_ = 0;
  }
  // For ordinary switches, 0 and 3 are unavailable/invalid two-bit values. A released switch
  // is 1, not 0; treating every nonzero value as pressed creates phantom clicks.
  void observe(uint16_t id, uint8_t dlc, const uint8_t *d, uint32_t now,
               bool synthetic = false) {
    if (!config_.enabled || synthetic || id != 0x3c2) return;
    if (!d || dlc != 8) { reset(); return; }
    if ((d[0] & 1) != 1) return; // Other multiplexors are independent traffic.
    if (!(d[6] & 0x10)) { reset(); return; } // Stock steering-input validity.
    if (seen_ && uint32_t(now - lastRx_) > config_.freshnessMs) reset();
    seen_ = true;
    lastRx_ = now;
    uint8_t values[] = {
      uint8_t((d[0] >> 5) & 3), uint8_t((d[1] >> 4) & 3),
      uint8_t((d[0] >> 3) & 3), uint8_t(d[1] & 3),
      uint8_t((d[1] >> 2) & 3), uint8_t((d[5] >> 6) & 3),
      uint8_t((d[2] >> 6) & 3), uint8_t(d[6] & 3),
      uint8_t((d[6] >> 2) & 3)
    };
    // High beam/wiper (DBC) and Camera (user-confirmed) use
    // INACTIVE/SOFT_PRESS/HARD_PRESS/FAULT, not
    // SNA/OFF/ON/FAULT. Soft -> hard is one continuous press.
    const uint8_t stagedButtons[]={uint8_t(MacroButton::HighBeam),uint8_t(MacroButton::Wiper),uint8_t(MacroButton::Camera)};
    for (uint8_t i : stagedButtons)
      values[i]=values[i]==0?1:(values[i]==3?0:2);
    for (uint8_t i = 0; i < uint8_t(MacroButton::Count); ++i)
      sample(i, values[i], now);
    poll(now);
  }
  void poll(uint32_t now) {
    if (!config_.enabled || !seen_) return;
    if (uint32_t(now - lastRx_) > config_.freshnessMs) { reset(); return; }
    for (uint8_t i = 0; i < uint8_t(MacroButton::Count); ++i) {
      auto &b = buttons_[i];
      if (!b.initialized || !b.armed) continue;
      // Do not emit an overdue double while a third press is debouncing.
      if (b.down && b.candidateDown && !b.longFired &&
          uint32_t(now - b.pressedAt) >= config_.longPressMs) {
        b.longFired = true;
        b.clicks = 0;
        emit(i, MacroGesture::Long);
      } else if (!b.down && !b.candidateDown && b.clicks &&
                 uint32_t(now - b.releasedAt) >= config_.clickGapMs) {
        if (b.clicks == 2) emit(i, MacroGesture::Double);
        b.clicks = 0;
      }
    }
  }
  bool popAction(uint8_t &action) {
    if (!count_) return false;
    action = queue_[head_];
    head_ = (head_ + 1) % QueueSize;
    --count_;
    return true;
  }
 private:
  struct ButtonState {
    bool initialized = false, armed = false, down = false;
    bool candidateDown = false, longFired = false;
    uint8_t clicks = 0;
    uint32_t candidateAt = 0, pressedAt = 0, releasedAt = 0;
  };
  static constexpr uint8_t QueueSize = 8;
  MacroGestureConfig config_{};
  ButtonState buttons_[uint8_t(MacroButton::Count)]{};
  bool seen_ = false;
  uint32_t lastRx_ = 0;
  uint8_t queue_[QueueSize]{}, head_ = 0, count_ = 0;
  void emit(uint8_t button, MacroGesture gesture) {
    for (const auto &s : config_.slots) {
      if (s.action && s.button == button && s.gesture == gesture && count_ < QueueSize) {
        queue_[(head_ + count_) % QueueSize] = s.action;
        ++count_;
      }
    }
  }
  void sample(uint8_t index, uint8_t value, uint32_t now) {
    auto &b = buttons_[index];
    if (value != 1 && value != 2) { b = ButtonState{}; return; }
    const bool down = value == 2;
    if (!b.initialized) {
      b.initialized = true;
      b.down = b.candidateDown = down;
      b.candidateAt = now;
      b.armed = !down; // A held button at startup must first be released.
      return;
    }
    if (down != b.candidateDown) {
      b.candidateDown = down;
      b.candidateAt = now;
      return;
    }
    if (down == b.down || uint32_t(now - b.candidateAt) < config_.debounceMs) return;
    b.down = down;
    if (!down && !b.armed) { b.armed = true; return; }
    if (!b.armed) return;
    if (down) {
      if (b.clicks && uint32_t(b.candidateAt - b.releasedAt) >= config_.clickGapMs) {
        if (b.clicks == 2) emit(index, MacroGesture::Double);
        b.clicks = 0;
      }
      b.pressedAt = b.candidateAt;
      b.longFired = false;
    } else {
      b.releasedAt = b.candidateAt;
      if (b.longFired) { b.clicks = 0; return; }
      // If no poll ran exactly at the threshold, a long release still counts
      // as long, provided the entire stream remained fresh.
      if (uint32_t(b.releasedAt - b.pressedAt) >= config_.longPressMs) {
        b.clicks = 0;
        b.longFired = true;
        emit(index, MacroGesture::Long);
      } else if (++b.clicks == 3) {
        b.clicks = 0;
        emit(index, MacroGesture::Triple);
      }
    }
  }
};
