#pragma once
static String featureActionsJsonArray() {
  String out="[";
  for(uint8_t id=0;id<=FEATURE_ACTION_LAST;++id){
    if(id)out+=',';
    String item;JsonWriterArduino w(item);
    w.u32("id",id);w.string("key",s3xyActionCode(id));w.string("label",s3xyActionLabel(id));
    w.boolean("supported",id==0||featureActionSupported(id));w.finish();out+=item;
  }
  out+=']';return out;
}
static void httpFeatureActions(){
  String out;JsonWriterArduino w(out);w.raw("actions",featureActionsJsonArray());
  w.u32("lastAction",featureActionLast);w.boolean("lastActionOk",featureActionLastOk);w.finish();server.send(200,"application/json",out);
}
static String convenienceJson() {
  ConvenienceConfig c;MacroGestureConfig m;convenienceSnapshot(c,m);
  String out;JsonWriterArduino w(out);
  w.boolean("masterEnabled",!canUsbPassive()&&!canUsbTxHeld());
  w.boolean("supported",convenienceBodySupported());w.boolean("macroSupported",convenienceMacroSupported());
  w.boolean("kickdownSupported",convenienceKickdownSupported());
  w.boolean("easyEntrySupported",convenienceActionSupported(FEATURE_ACTION_EASY_ENTRY));
  w.boolean("seatbeltTxSupported",convenienceBodySupported());w.boolean("nvsError",convenienceNvsError);
  w.boolean("kickdown",c.kickdown);w.boolean("easyEntry",c.easyEntry);w.boolean("mirrorSync",c.mirrorSync);
  w.boolean("rearBeltLeft",c.rearBeltLeft);w.boolean("rearBeltCenter",c.rearBeltCenter);w.boolean("rearBeltRight",c.rearBeltRight);
  w.boolean("reverseHazard",c.reverseHazard);w.boolean("reverseFog",c.reverseFog);
  w.u32("kickOnPct",c.kickOnPct);w.u32("kickOffPct",c.kickOffPct);w.u32("kickReleaseMode",c.kickReleaseMode);
  w.boolean("macrosEnabled",m.enabled);w.u32("debounceMs",m.debounceMs);w.u32("clickGapMs",m.clickGapMs);w.u32("longPressMs",m.longPressMs);
  String slots="[";
  for(unsigned n=0;n<3;++n){if(n)slots+=',';String item;JsonWriterArduino s(item);s.u32("button",m.slots[n].button);s.u32("gesture",uint8_t(m.slots[n].gesture));s.u32("action",m.slots[n].action);s.finish();slots+=item;}
  slots+=']';w.raw("slots",slots);w.raw("actions",featureActionsJsonArray());
  portENTER_CRITICAL(&convenienceMux);const uint32_t ok=convenienceTxOk,fail=convenienceTxFail;const bool active=convenienceEngine.kickdownActive();portEXIT_CRITICAL(&convenienceMux);
  portENTER_CRITICAL(&convenienceMux);
  const bool hazardReady=convenienceEngine.hazardThreeFlashReady(millis());
  const bool hazardActive=convenienceEngine.hazardThreeFlashActive();
  const uint8_t hazardFlashes=convenienceEngine.hazardThreeFlashCount();
  portEXIT_CRITICAL(&convenienceMux);
  w.boolean("hazardThreeFlashReady",hazardReady&&convenienceBodySupported()&&!canUsbTxHeld()&&!canTxAdministrativeHold&&mcpReady&&twaiReady);
  w.boolean("hazardThreeFlashActive",hazardActive);w.u32("hazardThreeFlashCount",hazardFlashes);
  w.u32("txOk",ok);w.u32("txFail",fail);w.boolean("kickdownActive",active);w.finish();return out;
}
static void httpConvenienceConfig(){server.send(200,"application/json",convenienceJson());}
static void httpConvenienceUpdate() {
  if(server.args()<1||server.args()>25){server.send(400,"application/json","{\"error\":\"invalid arguments\"}");return;}
  ConvenienceConfig c;MacroGestureConfig m;convenienceSnapshot(c,m);
  bool valid=true,unsupported=false,master=false,masterValue=false;
  for(int n=0;n<server.args()&&valid;++n){
    const String key=server.argName(n);const String raw=server.arg(n);
    for(int j=0;j<n;++j)if(server.argName(j)==key)valid=false;
    uint16_t v=0;if(!httpDecimalU16InRange(raw,0,5000,v)){valid=false;break;}
    bool *flag=nullptr;bool needsBody=true,needsKickdown=false;
    if(key=="masterEnabled"){master=true;masterValue=v==1;valid=valid&&v<=1&&server.args()==1;continue;}
    if(key=="kickdown"){flag=&c.kickdown;needsKickdown=true;}
    else if(key=="easyEntry"){flag=&c.easyEntry;}
    else if(key=="mirrorSync"){flag=&c.mirrorSync;}
    else if(key=="rearBeltLeft")flag=&c.rearBeltLeft;
    else if(key=="rearBeltCenter")flag=&c.rearBeltCenter;
    else if(key=="rearBeltRight")flag=&c.rearBeltRight;
    else if(key=="reverseHazard")flag=&c.reverseHazard;
    else if(key=="reverseFog")flag=&c.reverseFog;
    else if(key=="macrosEnabled"){flag=&m.enabled;needsBody=false;}
    if(flag){
      if(v>1){valid=false;break;}*flag=v==1;
      if(v&&(!(needsKickdown?convenienceKickdownSupported():(needsBody?convenienceBodySupported():convenienceMacroSupported()))))unsupported=true;
    }else if(key=="kickOnPct"){if(v>100)valid=false;else c.kickOnPct=v;}
    else if(key=="kickOffPct"){if(v>99)valid=false;else c.kickOffPct=v;}
    else if(key=="kickReleaseMode"){if(v>2)valid=false;else c.kickReleaseMode=v;}
    else if(key=="debounceMs")m.debounceMs=v;
    else if(key=="clickGapMs")m.clickGapMs=v;
    else if(key=="longPressMs")m.longPressMs=v;
    else if(key.startsWith("slot")&&key.length()>=10&&key.charAt(4)>='0'&&key.charAt(4)<='2'){
      auto &slot=m.slots[key.charAt(4)-'0'];const String field=key.substring(5);
      if(field=="Button"&&v<uint8_t(MacroButton::Count))slot.button=v;
      else if(field=="Gesture"&&(v==2||v==3||v==4))slot.gesture=MacroGesture(v);
      else if(field=="Action"&&v<=FEATURE_ACTION_LAST){slot.action=v;if(v&&!featureActionSupported(v))unsupported=true;}
      else valid=false;
    }else valid=false;
  }
  if(!valid||!convenienceConfigValidPure(c)||!macroGestureConfigValidPure(m)){server.send(400,"application/json","{\"error\":\"invalid settings\"}");return;}
  if(unsupported){server.send(409,"application/json","{\"error\":\"unavailable for this connection\"}");return;}
  const bool ok=master?canUsbRequestMode(!masterValue):convenienceApply(c,m);
  if(!ok){server.send(503,"application/json","{\"error\":\"save or controller transition failed\"}");return;}
  httpConvenienceConfig();
}

static void httpConvenienceWindowStatus() {
  String out;JsonWriterArduino w(out);const uint32_t now=millis();
  w.boolean("supported",convenienceBodySupported());w.boolean("labEnabled",labMenuEnabled);
  portENTER_CRITICAL(&convenienceMux);
  const bool ready=convenienceEngine.windowTestReady(now)&&!convenienceConfig.easyEntry;
  uint8_t doors[4],windows[4];uint32_t da[4],wa[4];
  for(unsigned n=0;n<4;++n){doors[n]=convenienceEngine.observedDoor(n);windows[n]=convenienceEngine.observedWindow(n);da[n]=convenienceEngine.doorAge(n,now);wa[n]=convenienceEngine.windowAge(n,now);}
  portEXIT_CRITICAL(&convenienceMux);
  w.boolean("ready",ready&&labMenuEnabled&&convenienceBodySupported()&&!canUsbTxHeld()&&mcpReady&&twaiReady);
  w.u32("sent",convenienceWindowTestCount);
  char bytes[32];snprintf(bytes,sizeof(bytes),"%02X %02X %02X %02X %02X %02X %02X %02X",convenienceWindowLastTest.data[0],convenienceWindowLastTest.data[1],convenienceWindowLastTest.data[2],convenienceWindowLastTest.data[3],convenienceWindowLastTest.data[4],convenienceWindowLastTest.data[5],convenienceWindowLastTest.data[6],convenienceWindowLastTest.data[7]);w.string("lastTx",bytes);
  String states="[";
  for(unsigned n=0;n<4;++n){if(n)states+=',';String item;JsonWriterArduino j(item);j.u32("door",doors[n]);j.u32("window",windows[n]);j.u32("doorAgeMs",da[n]);j.u32("windowAgeMs",wa[n]);j.finish();states+=item;}
  states+=']';w.raw("states",states);w.finish();server.send(200,"application/json",out);
}
static void httpConvenienceWindowTest() {
  uint16_t type=0,percent=0,corner=0;bool valid=server.args()==3;unsigned seen=0;
  for(int n=0;n<server.args()&&valid;++n){
    const String key=server.argName(n);uint16_t v=0;unsigned bit=0;
    if(!httpDecimalU16InRange(server.arg(n),0,100,v)){valid=false;break;}
    if(key=="command"){type=v;bit=1;}else if(key=="percent"){percent=v;bit=2;}else if(key=="window"){corner=v;bit=4;}else valid=false;
    if(seen&bit)valid=false;
    seen|=bit;
  }
  if(!valid||seen!=7||type<1||type>4||percent>100||percent%5||corner>3){server.send(400,"application/json","{\"error\":\"invalid test command\"}");return;}
  if(!convenienceWindowTest(type,percent,corner)){server.send(409,"application/json","{\"error\":\"Requires LAB, Juniper Body, transmission enabled, Park, fresh idle window frame and Easy Entry OFF\"}");return;}
  httpConvenienceWindowStatus();
}

static void httpConvenienceHazardThreeFlash() {
  if(server.args()){server.send(400,"application/json","{\"error\":\"no arguments allowed\"}");return;}
  if(!convenienceHazardThreeFlash()){server.send(409,"application/json","{\"error\":\"Requires Juniper Body, transmission enabled, fresh idle lights and released hazard button; unavailable in Reverse or while busy\"}");return;}
  httpConvenienceConfig();
}
