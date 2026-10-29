#include "../convenience_pure.h"
#include <cassert>
#include <cstdio>
struct Fixture {
 ConvenienceEngine e;ConvenienceConfig c;uint32_t now=100,epoch=1;
 ConvenienceOutput send(uint16_t id,const uint8_t* d,uint8_t dlc=8,bool tx=true){return e.process(c,id,d,dlc,now,epoch,tx);}
 ConvenienceOutput gear(uint8_t g,uint8_t pedal=0){uint8_t d[8]={};d[2]=g<<5;d[4]=pedal;return send(0x118,d);}
 void brake(uint8_t v){uint8_t d[8]={};d[2]=v;send(0x39D,d);}
 void ack(const ConvenienceOutput& o,bool ok=true){for(uint8_t i=0;i<o.count;++i)e.acknowledge(o.frames[i],ok);}
};
static void kickdown(){
 Fixture f;f.c.kickdown=true;uint8_t stock[8]={0x1f,1,2,3,4,5,6,7};
 f.gear(4,225);f.brake(1);assert(f.send(0x334,stock).count==0); // strict >90%
 f.gear(4,226);auto o=f.send(0x334,stock);assert(o.count==1&&o.frames[0].data[0]==0x3f);
 for(int i=1;i<7;++i)assert(o.frames[0].data[i]==stock[i]);
 assert(o.frames[0].data[7]==0x8b); // 0x03+0x34+0x3f+1+2+3+4+5+6
 assert(f.send(0x334,o.frames[0].data).count==0); // echo idempotence
 f.gear(4,24);assert(f.send(0x334,stock).count==0);
 f.c.kickOnPct=60;f.c.kickOffPct=30;f.gear(4,151);assert(f.send(0x334,stock).count==1);
 f.brake(2);assert(f.send(0x334,stock).count==0);
 f.brake(1);f.gear(4,151);assert(f.send(0x334,stock).count==1);
 f.now+=251;assert(f.send(0x334,stock).count==0&&!f.e.kickdownActive());
 f.gear(4,151);f.brake(1);assert(f.send(0x334,stock,7).count==0);
 f.c.kickReleaseMode=1;assert(f.send(0x334,stock).count==1);f.gear(4,0);assert(f.send(0x334,stock).count==1);
 f.brake(2);assert(f.send(0x334,stock).count==0);
 f.gear(7,250);f.brake(1);assert(f.send(0x334,stock).count==0);
 f.gear(4,255);assert(f.send(0x334,stock).count==0);
 f.c.kickOffPct=90;assert(!convenienceConfigValidPure(f.c));assert(f.send(0x334,stock).count==0);
}
static void belts(){
 Fixture f;uint8_t d[8]={0,0xaa,0xbb,0xcc,0xdd,0x55,0x90,0x42};
 assert(f.send(0x3C2,d).count==0); // disabled by default
 f.c.rearBeltLeft=true;f.c.rearBeltCenter=true;
 auto o=f.send(0x3C2,d);assert(o.count==1);assert(o.frames[0].data[6]==0xa0&&o.frames[0].data[7]==0x82);
 // Runtime removes transmitted echoes before the stock observer.
 d[0]=2;assert(f.send(0x3C2,d).count==1); // bit1 is reserved, MUX is bit0
 d[0]=1;assert(f.send(0x3C2,d).count==0);d[0]=0;assert(f.send(0x3C2,d,7).count==0);
 f.c.rearBeltRight=true;o=f.send(0x3C3,d);assert(o.count==1&&o.frames[0].data[5]==0x95);
 for(int i=0;i<8;++i)if(i!=5)assert(o.frames[0].data[i]==d[i]);
 assert(f.send(0x3C3,o.frames[0].data).count==0);
 d[6]=0xb0;d[7]=0xc2;assert(f.send(0x3C2,d).count==0); // FAULT stays FAULT
 d[6]=0x80;d[7]=2;assert(f.send(0x3C2,d).count==0); // SNA stays SNA
 d[5]=0xd5;assert(f.send(0x3C3,d).count==0);
}
static void composedMux(){
 Fixture f;f.c.reverseHazard=true;f.c.rearBeltLeft=true;uint8_t d[8]={};f.gear(1);f.send(0x3F5,d);f.gear(2);d[6]=0x10;d[7]=2;auto o=f.send(0x3C2,d);
 assert(o.count==1&&o.frames[0].data[0]==8&&(o.frames[0].data[6]&0x30)==0x20);
}
static void reverse(){
 Fixture f;f.c.reverseHazard=true;f.c.reverseFog=true;uint8_t d[8]={};
 f.gear(1);f.send(0x3F5,d);f.now+=10;f.gear(2);
 auto o=f.send(0x3C2,d);assert(o.count==1&&o.frames[0].data[0]==8);f.ack(o);assert(f.e.hazardOwned());
 // Runtime removes transmitted echoes before the stock observer.
 assert(f.send(0x273,d).count==1);
 f.now+=201;f.gear(2);assert(f.send(0x3C2,d).count==0); // no repeated ON
 f.now+=10;f.gear(4);assert(f.send(0x3C2,d).count==0);f.now+=400;f.gear(4);f.now+=390;f.gear(4);o=f.send(0x3C2,d);assert(o.count==1&&o.frames[0].effect==4);f.ack(o);assert(!f.e.hazardOwned());
 assert(f.send(0x273,d).count==0);
 f.now+=201;f.gear(4);assert(f.send(0x3C2,d).count==0);
 Fixture failed;failed.c.reverseHazard=true;failed.gear(1);failed.send(0x3F5,d);failed.gear(2);o=failed.send(0x3C2,d);failed.ack(o,false);
 failed.now+=201;failed.gear(4);assert(failed.send(0x3C2,d).count==0); // failed ON cannot turn stock hazards OFF
 Fixture physical;physical.c.reverseHazard=true;physical.gear(1);d[6]=0x14;physical.send(0x3F5,d);physical.gear(2);d[6]=0;assert(physical.send(0x3C2,d).count==0);
 f.gear(2);f.now+=501;assert(f.send(0x273,d).count==0); // stale gear
 Fixture manual;manual.c.reverseHazard=true;manual.gear(1);manual.send(0x3F5,d);manual.gear(2);o=manual.send(0x3C2,d);manual.ack(o);d[0]=8;manual.send(0x3C2,d);assert(!manual.e.hazardOwned());d[0]=0;
 Fixture boot;boot.c.reverseHazard=true;boot.send(0x3F5,d);boot.gear(2);assert(boot.send(0x3C2,d).count==0); // no synthetic boot edge
}
static void setupEntry(Fixture& f){
 f.c.easyEntry=true;f.c.windowDropRaw=20;f.gear(1);uint8_t d[8]={};f.send(0x61D,d);f.send(0x294,d);f.send(0x273,d);d[0]=1;d[6]=0x12;f.send(0x2C2,d);d[0]=0;d[6]=0;d[2]=0x12;f.send(0x122,d);d[2]=0;f.send(0x102,d);
}
static void easyEntry(){
 Fixture unverified;setupEntry(unverified);unverified.c.windowDropRaw=255;uint8_t handle[8]={0,4};assert(unverified.send(0x102,handle).count==0);
 Fixture f;setupEntry(f);uint8_t d[8]={};d[1]=4;auto o=f.send(0x102,d);assert(o.count==1&&o.frames[0].id==0x294&&o.frames[0].data[5]==80);assert(o.frames[0].data[6]==0x11);f.ack(o);
 assert(f.send(0x102,d).count==0);d[1]=0;o=f.send(0x102,d);assert(o.count==0); // DBC has no STOP=6
 d[2]=0x18;f.send(0x122,d);d[2]=0x12;o=f.send(0x122,d);assert(o.count==1&&o.frames[0].data[6]==0x31);f.ack(o);
 d[2]=0x18;f.send(0x122,d);d[2]=0x12;assert(f.send(0x122,d).count==0);
 Fixture failed;setupEntry(failed);d[1]=4;d[2]=0;o=failed.send(0x102,d);failed.ack(o,false);d[2]=0x18;failed.send(0x122,d);d[2]=0x12;assert(failed.send(0x122,d).count==0);
 Fixture wet;setupEntry(wet);d[7]=1;wet.send(0x61D,d);d[1]=4;assert(wet.send(0x102,d).count==0);
 Fixture stale;setupEntry(stale);stale.now+=2001;assert(stale.send(0x102,d).count==0);
 Fixture invalid;setupEntry(invalid);invalid.gear(7);assert(invalid.send(0x102,d).count==0);
 Fixture unknown;unknown.c.easyEntry=true;unknown.gear(1);assert(unknown.send(0x102,d).count==0);
}
static void dbcEntryAndTemplate(){
 Fixture f;setupEntry(f);uint8_t stock[8]={0xa5,0x5a,0x22,0x33,0x44,3,0x80,0xe7};
 f.send(0x294,stock);uint8_t handle[8]={0,4};auto o=f.send(0x102,handle);
 assert(o.count==1&&o.frames[0].data[5]==0x53&&o.frames[0].data[6]==0x91);
 for(int i=0;i<8;++i)if(i!=5&&i!=6)assert(o.frames[0].data[i]==stock[i]);
 // Missing stock template, stock UI command, wrong ID and wrong MUX cannot lower.
 Fixture no;no.c.easyEntry=true;no.gear(1);uint8_t d[8]={};no.send(0x61d,d);
 d[0]=1;d[6]=0x12;no.send(0x2c2,d);d[0]=0;d[6]=0;d[2]=0x12;no.send(0x122,d);d[2]=0;no.send(0x102,d);
 assert(no.send(0x102,handle).count==0);
 Fixture busy;setupEntry(busy);stock[6]=0x10;busy.send(0x294,stock);assert(busy.send(0x102,handle).count==0);
 Fixture wrongMux;setupEntry(wrongMux);d[0]=1;d[2]=0;wrongMux.send(0x122,d);assert(wrongMux.send(0x102,handle).count==1);
 Fixture partial;setupEntry(partial);d[0]=1;d[6]=8;partial.send(0x2c2,d);assert(partial.send(0x102,handle).count==0);
 Fixture oldId;setupEntry(oldId);d[0]=1;d[6]=0;oldId.send(0x2c6,d);assert(oldId.send(0x102,handle).count==1);
 Fixture invalidLights;invalidLights.c.reverseHazard=true;invalidLights.gear(1);d[6]=0x0c;invalidLights.send(0x3f5,d);invalidLights.gear(2);d[0]=0;assert(invalidLights.send(0x3c2,d).count==0);
}
static void gatesAndWrap(){
 Fixture f;f.c.kickdown=true;f.now=0xfffffff0u;f.gear(4,250);f.brake(1);f.now=5;uint8_t d[8]={};assert(f.send(0x334,d).count==1);
 assert(f.send(0x334,d,8,false).count==0);assert(f.send(0x334,d).count==0);
 f.gear(4,250);f.brake(1);assert(f.send(0x334,d).count==1);++f.epoch;assert(f.send(0x334,d).count==0);
 assert(f.send(0x334,nullptr).count==0);assert(f.send(0x334,d,9).count==0);
}
int main(){kickdown();belts();composedMux();reverse();easyEntry();dbcEntryAndTemplate();gatesAndWrap();std::puts("Convenience pure: kickdown, belt field isolation, reverse edges, window ownership, stale/epoch/gate/wrap PASS");}
