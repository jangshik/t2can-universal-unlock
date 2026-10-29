#include "../macro_gesture_pure.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct Fixture {
  MacroGestureEngine e;
  uint8_t d[8] = {0x29, 0x15, 0x40, 0, 0, 0x40, 0x15, 0};
  uint32_t now = 0;
  Fixture() {
    MacroGestureConfig c;
    c.enabled = true;
    c.slots[0] = {0, MacroGesture::Double, 10};
    c.slots[1] = {0, MacroGesture::Triple, 11};
    c.slots[2] = {0, MacroGesture::Long, 12};
    assert(e.configure(c));
    rx();
  }
  void rx(bool synthetic = false) { e.observe(0x3c2, 8, d, now, synthetic); }
  void wait(uint32_t ms) {
    while (ms) { const uint32_t step = ms > 20 ? 20 : ms; now += step; ms -= step; rx(); }
  }
  void state(uint8_t v) { d[0] = (d[0] & ~0x60) | (v << 5); rx(); }
  void click() { state(2); wait(60); state(1); wait(40); }
  uint8_t pop() { uint8_t a = 0; assert(e.popAction(a)); return a; }
  void empty() { uint8_t a = 0; assert(!e.popAction(a)); }
};
int main() {
  { Fixture f; f.wait(1000); f.empty(); f.click(); f.wait(500); f.empty(); }
  { Fixture f; f.click(); f.click(); f.wait(200); f.empty(); f.wait(160); assert(f.pop()==10); f.empty(); }
  { Fixture f; f.click(); f.click(); f.click(); assert(f.pop()==11); f.wait(500); f.empty(); }
  { Fixture f; f.state(2); f.wait(799); f.empty(); f.wait(1); assert(f.pop()==12);
    f.wait(2500); f.empty(); f.state(1); f.wait(500); f.empty(); }
  { Fixture f; f.e.reset(); f.state(2); f.wait(1200); f.empty(); f.state(1); f.wait(40);
    f.click(); f.click(); f.wait(400); assert(f.pop()==10); }
  { Fixture f; f.click(); f.state(2); f.wait(10); f.state(1); f.wait(500); f.empty(); }
  { Fixture f; f.click(); f.click(); f.now += 251; f.e.poll(f.now); f.wait(500); f.empty(); }
  { Fixture f; f.click(); f.click(); f.now += 251; f.rx(); f.wait(500); f.empty(); }
  { Fixture f; f.click(); f.state(0); f.wait(40); f.state(1); f.wait(40); f.click(); f.wait(500); f.empty(); }
  { Fixture f; f.click(); f.click(); f.e.reset(); f.wait(500); f.empty(); }
  { Fixture f; f.now = UINT32_MAX - 100; f.e.reset(); f.rx(); f.click(); f.click(); f.wait(400); assert(f.pop()==10); }
  { Fixture f; f.click(); f.click(); f.now += 400; f.rx(true); f.e.poll(f.now); f.empty(); }
  { Fixture f; f.click(); f.click(); f.e.observe(0x3c2, 7, f.d, f.now); f.wait(500); f.empty(); }
  { Fixture f; f.click(); f.click(); f.d[6] &= ~0x10; f.rx(); f.wait(500); f.empty(); }
  { Fixture f; auto c = f.e.config(); c.enabled = false; assert(f.e.configure(c));
    f.click(); f.click(); f.click(); f.wait(1000); f.empty(); }
  { Fixture f; auto c = f.e.config(); c.longPressMs=200; c.clickGapMs=100; c.debounceMs=5;
    assert(f.e.configure(c)); f.rx(); f.state(2); f.wait(200); assert(f.pop()==12);
    c.clickGapMs=0; assert(!f.e.configure(c)); assert(f.e.config().clickGapMs==100); }
  // Third press begins inside the gap, even if debounce completes after it.
  { Fixture f; f.click(); f.click(); f.wait(300); f.state(2); f.wait(40); f.empty();
    f.state(1); f.wait(40); assert(f.pop()==11); f.empty(); }
  // A held press is long once; its eventual release cannot complete a double.
  { Fixture f; f.click(); f.state(2); f.wait(900); assert(f.pop()==12);
    f.state(1); f.wait(500); f.empty(); }
  { Fixture f;auto c=f.e.config();c.slots[0].button=8;assert(!f.e.configure(c)); }
  // DBC: inactive=0; soft=1 and hard=2 both count as pressed. The
  // soft-to-hard transition must not create a second click.
  for (uint8_t button=5;button<=6;++button) {
    Fixture f;auto c=f.e.config();c.slots[0]={button,MacroGesture::Double,20};
    c.slots[1]={button,MacroGesture::Triple,21};c.slots[2]={button,MacroGesture::Long,22};
    assert(f.e.configure(c));const uint8_t byte=button==5?5:2;
    auto state=[&](uint8_t v){f.d[byte]=(f.d[byte]&0x3f)|(v<<6);f.rx();};
    state(0);f.wait(40);f.wait(900);f.empty();
    for(int n=0;n<2;++n){state(1);f.wait(40);state(2);f.wait(40);state(0);f.wait(40);}
    f.wait(400);assert(f.pop()==20);f.empty();
    state(1);f.wait(800);assert(f.pop()==22);f.wait(400);f.empty();
    state(3);f.wait(40);state(0);f.wait(40);f.empty();
  }
  puts("macro gestures: debounce, double/triple arbitration, long, stale, invalid, wrap PASS");
}
