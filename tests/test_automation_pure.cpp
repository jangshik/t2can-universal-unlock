#include "../automation_pure.h"
#include <cassert>
#include <cstdio>

static AutomationSensors sample(uint32_t at,int speed=0){
  AutomationSensors s;s.speed.set(speed*100,at);s.gear.set(4,at);s.brake.set(1,at);s.camera.set(0,at);s.fascia.set(0,at);return s;
}
static void rules(){
  AutomationConfig c;c.rules[0].enabled=true;assert(automationConfigValid(c,54));
  AutomationEngine e;auto s=sample(100);assert(!e.evaluate(c,s,100).entered);
  s=sample(101,1);assert(e.evaluate(c,s,101).entered==1); // Inclusive lower bound.
  s=sample(102,20);assert(!e.evaluate(c,s,102).entered);
  s=sample(103,21);assert(e.evaluate(c,s,103).active==1);
  for(uint32_t t=203;t<=3003;t+=100){s=sample(t,21);assert(!e.evaluate(c,s,t).left);}
  s=sample(3103,21);assert(e.evaluate(c,s,3103).left==1);
  s=sample(3104,10);assert(e.evaluate(c,s,3104).entered==1);
  s=sample(3105,0);e.evaluate(c,s,3105);s=sample(3300,10);assert(e.evaluate(c,s,3300).active==1);
  // Lost input cancels pending work; fresh recovery is a baseline, not replay.
  assert(e.evaluate(c,s,4000).invalid&1);s=sample(4001,10);assert(!e.evaluate(c,s,4001).entered);
  c.rules[1]=c.rules[0];assert(!automationConfigValid(c,54));c.rules[1].enabled=false;
  c.rules[0].low=21;assert(!automationConfigValid(c,54));c.rules[0].low=1;
  c.rules[0].sensor=2;assert(!automationConfigValid(c,54));
  c.rules[0].action=54;c.rules[0].low=c.rules[0].high=2;c.rules[0].gear=0;c.rules[0].enterMs=100;c.rules[0].exitMs=0;
  assert(automationConfigValid(c,54));e.reset();s=sample(UINT32_MAX-50);s.gear.set(4,UINT32_MAX-50);e.evaluate(c,s,UINT32_MAX-50);
  s.gear.set(2,UINT32_MAX-40);e.evaluate(c,s,UINT32_MAX-40);s.gear.set(2,60);assert(e.evaluate(c,s,60).entered==1);
  s.gear.set(4,61);assert(e.evaluate(c,s,61).left==1);
}
static void sixteenRules(){
 AutomationConfig c;AutomationEngine e;
 for(auto&r:c.rules){r.enabled=true;r.action=54;r.enterMs=100;}
 auto s=sample(100,0);e.evaluate(c,s,100);s=sample(101,10);assert(!e.evaluate(c,s,101).entered);
 e.resetRule(15);s=sample(150,10);e.evaluate(c,s,150);
 s=sample(201,10);auto edges=e.evaluate(c,s,201);assert(edges.entered==0x7fff&&edges.active==0xffff);
 assert(e.evaluate(c,s,702).invalid==0xffff);
 e.reset();s=sample(800,0);e.evaluate(c,s,800);s=sample(801,10);e.evaluate(c,s,801);s=sample(901,10);assert(e.evaluate(c,s,901).entered==0xffff);
}
static void decoding(){
  AutomationSensors s;uint8_t d[8]={};const uint16_t raw=625;d[5]=uint8_t(raw<<3);d[6]=raw>>5;s.observe(0x257,d,8,10);assert(s.speed.value==1000&&s.speed.valid);
  d[5]=d[6]=255;s.observe(0x257,d,8,11);assert(!s.speed.valid);
  d[5]=2;s.observe(0x353,d,8,20);d[4]=0;s.observe(0x294,d,8,20);assert(s.cameraFresh(20)&&s.cameraOn());
}
static void camera(){
  AutomationCamera c;uint8_t stock[8]={1,0x15,0x40,0,0,0x40,0x10,0xa5},out[8];
  auto s=sample(100,10);c.observe(0x3c2,stock,8,100);c.maintain(s,100,true);
  s.camera.set(1,100);assert(!c.open(s,100));s.camera.set(0,100);assert(c.open(s,100));
  assert(c.prepare(out,100)==1&&out[6]==0x14);for(unsigned i=0;i<8;++i)if(i!=6)assert(out[i]==stock[i]);
  c.acknowledge(1,true,100);assert(!c.owned());
  s=sample(150,10);s.fascia.set(1,150);c.observe(0x3c2,stock,8,150);c.maintain(s,150,true);assert(c.owned());
  assert(c.prepare(out,200)==2&&out[6]==stock[6]);c.acknowledge(2,true,200);assert(!c.busy()&&c.owned());
  assert(c.close(s,200));c.acknowledge(c.prepare(out,200),true,200);c.observe(0x3c2,stock,8,300);assert(c.prepare(out,300)==2);c.acknowledge(2,true,300);assert(!c.owned());
  // A physical press relinquishes ownership, including between prepare and ACK.
  s=sample(400,10);c.observe(0x3c2,stock,8,400);assert(c.open(s,400));assert(c.prepare(out,400)==1);
  stock[6]|=4;c.observe(0x3c2,stock,8,401);c.acknowledge(1,true,402);assert(!c.owned()&&!c.busy());stock[6]&=~4;
  c.observe(0x3c2,stock,8,500);s=sample(500,10);assert(c.open(s,500));c.acknowledge(1,false,500);assert(!c.busy());
  c.observe(0x3c2,stock,8,600);s=sample(600,10);assert(c.open(s,600));c.acknowledge(1,true,600);s.gear.set(2,601);c.maintain(s,601,true);assert(!c.busy()&&!c.owned());
  c.observe(0x3c2,stock,8,700);s=sample(700,10);assert(c.open(s,700));c.acknowledge(1,true,700);c.maintain(s,1701,true);assert(!c.busy());
}
int main(){rules();sixteenRules();decoding();camera();puts("automation: edges, delays, re-entry, stale recovery, wrap, camera ownership and manual cancellation PASS");}
