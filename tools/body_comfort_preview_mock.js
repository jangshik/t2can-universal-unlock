// Offline preview mock for the Body Comfort panel and Wi-Fi timing card.
// Simulates a HW4 Model Y Juniper (Body + Chassis) and mirrors the firmware
// behaviour of body_comfort_runtime.h closely enough to try every option.
(()=>{
  const base=window.fetch;
  const json=(v,status=200)=>Promise.resolve(new Response(JSON.stringify(v),{status,headers:{'Content-Type':'application/json'}}));
  const cfg={eeEnabled:false,eeDoorMask:15,eeWeatherBlock:true,eeWeatherStrict:false,eeRestoreDelayMs:800,eeMirrorFold:false,eeMirrorCamera:true,eeUnfoldKph:10,eeUnfoldHoldS:3,sbEnabled:false,sbSeatMask:7,mcEnabled:false,mcLongMs:800,mcGapMs:400,mcAction:[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],hzBlinkEnabled:false,hzBlinkSeconds:3,rvHazard:false,rvLight:false,rvLightMode:2,drHazard:false,drOffDelayMs:2000,kdEnabled:false,kdTriggerPct:90,kdTargetRaw:1,kdReleaseOnPedal:true,kdReleasePct:20,kdReleaseOnBrake:true,kdReleaseMode:0,dhEnabled:false,dhDoorMask:1,dhHoldS:2,dhAction:20};
  const ranges={eeDoorMask:[0,15],eeRestoreDelayMs:[0,5000],eeUnfoldKph:[1,80],eeUnfoldHoldS:[0,30],sbSeatMask:[0,7],mcLongMs:[500,2000],mcGapMs:[200,800],hzBlinkSeconds:[1,30],rvLightMode:[0,2],drOffDelayMs:[0,10000],kdTriggerPct:[10,100],kdTargetRaw:[1,2],kdReleasePct:[0,90],kdReleaseMode:[0,1],dhDoorMask:[0,3],dhHoldS:[1,10],dhAction:[0,22]};
  const wifi={ok:true,slowStartEnabled:false,startDelayS:30,hideEnabled:false,hideDelayS:300,hiddenNow:false,startDelayAppliedS:0,apUptimeS:0,setupMode:false};
  const wifiStart=Date.now();
  const DOORS=['FL','FR','RL','RR'];
  const legacy={};
  let lastMacro={button:0,gesture:0,action:0,result:''};
  const car={gear:'P',kph:0,door:[false,false,false,false],win:[0,0,0,0],rain:false,snow:false,mirrors:'UNFOLDED',camera:false,hazard:'OFF',belts:[false,false,false],pedal:0,brake:false,handle:[false,false],frunk:false,pedalMode:'Comfort (Chill)',listenOnly:false,locked:false,log:[]};
  const st={entry:['IDLE','IDLE','IDLE','IDLE'],saved:[0,0,0,0],foldedByUs:false,cameraByUs:false,unfoldSince:0,hazardOwned:false,userHazard:false,blinkUntil:0,doorHazUntil:0,doorHazActive:false,rearLight:false};
  const stats={txOk:0,txFail:0,entryOpens:0,entryRestores:0,entryBlockedWeather:0,entryBlockedGear:0,seatbeltInjected:0,macroEvents:0,hazardPresses:0,blinkRuns:0,mirrorFolds:0,mirrorUnfolds:0,cameraPresses:0};
  const log=msg=>{car.log.unshift(new Date().toLocaleTimeString()+'  '+msg);car.log.length=Math.min(car.log.length,12);window.__bcPreviewRender?.()};
  const tx=(what)=>{if(car.listenOnly){log('(listen-only, not sent) '+what);return}stats.txOk++;log('TX '+what)};
  const posName=p=>p<=0?'CLOSED':p>=100?'OPEN':p<=10?'VENT':'PARTIAL';
  function setWindow(i,p,why){car.win[i]=p;tx(`0x294 ${DOORS[i]} ${p===100?'GOTO_OPEN':p===0?'GOTO_CLOSED':'GOTO_PERCENT '+p+'%'}${why?' · '+why:''}`)}
  function hazardWanted(){return(cfg.rvHazard&&car.gear==='R')||st.doorHazActive||Date.now()<st.blinkUntil}
  function hazardService(){
    const want=hazardWanted();
    if(want&&car.hazard==='OFF'&&!st.userHazard){car.hazard='ON';st.hazardOwned=true;stats.hazardPresses++;tx('0x3C2 mux0 hazard button press (ON)')}
    if(!want&&st.hazardOwned&&car.hazard==='ON'){car.hazard='OFF';st.hazardOwned=false;stats.hazardPresses++;tx('0x3C2 mux0 hazard button press (OFF)')}
    if(!want)st.userHazard=false;
  }
  function doorOpened(i){
    if(cfg.drHazard&&(car.gear==='D'||car.gear==='R')){st.doorHazActive=true;st.doorHazUntil=0}
    if(!cfg.eeEnabled||!((cfg.eeDoorMask>>i)&1))return;
    if(car.gear!=='P'){stats.entryBlockedGear++;log(`Easy Entry ${DOORS[i]} blocked: gear ${car.gear}`);return}
    if(cfg.eeWeatherBlock&&(car.rain||car.snow)){stats.entryBlockedWeather++;log(`Easy Entry ${DOORS[i]} blocked: ${car.rain?'rain':'snow'}`);return}
    if(car.win[i]>=100)return;
    st.saved[i]=car.win[i];st.entry[i]='OPENED';stats.entryOpens++;setWindow(i,100,'Easy Entry');
    if(cfg.eeMirrorFold&&car.mirrors==='UNFOLDED'&&!st.foldedByUs){car.mirrors='FOLDED';st.foldedByUs=true;stats.mirrorFolds++;tx('0x273 UI_mirrorFoldRequest=RETRACT')}
  }
  function doorClosed(i){
    if(st.entry[i]==='OPENED'){
      st.entry[i]='RESTORING';
      setTimeout(()=>{if(st.entry[i]!=='RESTORING'||car.door[i])return;st.entry[i]='IDLE';stats.entryRestores++;setWindow(i,st.saved[i],'restore');},cfg.eeRestoreDelayMs);
    }
    if(st.doorHazActive&&!car.door.some(Boolean))st.doorHazUntil=Date.now()+cfg.drOffDelayMs;
  }
  const kd={kicked:false,belowSince:0};const hd=[{since:0,fired:false},{since:0,fired:false}];
  const MODE=['Comfort (Chill)','Standard','Sport'];
  function kickdownTick(now){
    if(!cfg.kdEnabled){if(kd.kicked){kd.kicked=false;car.pedalMode=MODE[0];tx('0x334 UI_pedalMap → Chill (kickdown off)')}return}
    if(kd.kicked){
      let rel=car.gear!=='D'||(cfg.kdReleaseOnBrake&&car.brake);
      if(!rel&&cfg.kdReleaseOnPedal&&car.pedal<=cfg.kdReleasePct){if(!kd.belowSince)kd.belowSince=now;if(now-kd.belowSince>=500)rel=true}else kd.belowSince=0;
      if(rel){kd.kicked=false;kd.belowSince=0;car.pedalMode=cfg.kdReleaseMode?'Vehicle setting':MODE[0];tx(`0x334 UI_pedalMap → ${cfg.kdReleaseMode?'stock':'Chill'} (kickdown release)`)}
    }else if(car.gear==='D'&&!car.brake&&car.pedal>=cfg.kdTriggerPct){kd.kicked=true;car.pedalMode=MODE[cfg.kdTargetRaw];tx(`0x334 UI_pedalMap → ${MODE[cfg.kdTargetRaw]} (kickdown)`)}
  }
  function handleTick(now){
    for(let i=0;i<2;i++){const h=hd[i];
      if(!cfg.dhEnabled||!((cfg.dhDoorMask>>i)&1)||!car.handle[i]){h.since=0;h.fired=false;continue}
      // Same gate as the firmware: unlocked and that front door open.
      if(car.locked||!car.door[i]){h.since=0;continue}
      if(!h.since)h.since=now;
      if(!h.fired&&now-h.since>=cfg.dhHoldS*1000){h.fired=true;stats.macroEvents++;lastMacro={button:8+i,gesture:2,action:cfg.dhAction,result:'OK'};const r=runAction(cfg.dhAction);lastMacro.result=r||'OK';if(r)log('Door handle: '+r)}
    }
  }
  function tick(){
    kickdownTick(Date.now());handleTick(Date.now());
    const now=Date.now();
    if(st.doorHazActive&&st.doorHazUntil&&now>=st.doorHazUntil&&!car.door.some(Boolean)){st.doorHazActive=false;st.doorHazUntil=0}
    if(!cfg.drHazard)st.doorHazActive=false;
    st.rearLight=cfg.rvLight&&car.gear==='R';
    if(st.foldedByUs){
      if(cfg.eeMirrorCamera&&(car.gear==='D'||car.gear==='N')&&!car.camera&&!st.cameraByUs){car.camera=true;st.cameraByUs=true;stats.cameraPresses++;tx('0x3C2 mux1 camera button press')}
      if(car.gear==='D'&&car.kph>cfg.eeUnfoldKph){if(!st.unfoldSince)st.unfoldSince=now;if(now-st.unfoldSince>=cfg.eeUnfoldHoldS*1000){car.mirrors='UNFOLDED';st.foldedByUs=false;st.unfoldSince=0;stats.mirrorUnfolds++;tx('0x273 UI_mirrorFoldRequest=PRESENT');if(st.cameraByUs&&car.camera){car.camera=false;stats.cameraPresses++;tx('0x3C2 mux1 camera button press (close)')}st.cameraByUs=false}}
      else st.unfoldSince=0;
    }
    if(cfg.sbEnabled)car.belts.forEach((u,s)=>{if(u&&((cfg.sbSeatMask>>s)&1))stats.seatbeltInjected++});
    hazardService();
    window.__bcPreviewRender?.();
  }
  setInterval(tick,250);
  function runAction(a){
    const toggles={1:'eeEnabled',2:'eeMirrorFold',3:'sbEnabled',5:'rvHazard',6:'rvLight',7:'drHazard'};
    const existing={11:'Nag Killer',12:'ISA Suppression',13:'Disable Driver Monitoring',14:'Auto Blinker',15:'TLSSC',16:'Door Button Cancel',17:'Visual Speed Control',18:'Continuous AP',19:'Battery Preheating'};
    if(existing[a]){legacy[a]=!legacy[a];log(`Macro: ${existing[a]} → ${legacy[a]?'ON':'OFF'} (기존 기능)`);return null}
    if(toggles[a]){cfg[toggles[a]]=!cfg[toggles[a]];log(`Macro: ${toggles[a]} → ${cfg[toggles[a]]?'ON':'OFF'}`);return null}
    if(a===8){if(!cfg.hzBlinkEnabled)return'hazard blink disabled';if(car.hazard!=='OFF')return'hazards already on';st.blinkUntil=Date.now()+cfg.hzBlinkSeconds*1000;stats.blinkRuns++;log(`Hazard blink ${cfg.hzBlinkSeconds}s`);return null}
    if(a===9){if(car.mirrors==='FOLDED'){car.mirrors='UNFOLDED';st.foldedByUs=false;tx('0x273 UI_mirrorFoldRequest=PRESENT')}else{car.mirrors='FOLDED';st.foldedByUs=true;tx('0x273 UI_mirrorFoldRequest=RETRACT')}return null}
    if(a===10){car.camera=!car.camera;stats.cameraPresses++;tx('0x3C2 mux1 camera button press');return null}
    if(a===20){if(car.gear!=='P')return'frunk only opens in P';car.frunk=true;tx('0x273 UI_frunkRequest=MOVE (frunk open)');return null}
    if(a===21){car.listenOnly=!car.listenOnly;log('Listen-only → '+(car.listenOnly?'ON':'OFF'));return null}
    if(a===22){cfg.kdEnabled=!cfg.kdEnabled;log('Macro: kdEnabled → '+(cfg.kdEnabled?'ON':'OFF'));return null}
    return'no action';
  }
  function body(){
    return{ok:true,supported:true,bus:'BODY_CAN_A',layout:'JUNIPER_VEH',taskRunning:true,cfg:JSON.parse(JSON.stringify(cfg)),
      live:{gear:car.gear,speedKph:car.kph,weather:car.rain&&car.snow?'RAIN+SNOW':car.rain?'RAIN':car.snow?'SNOW':'CLEAR',
        doors:DOORS.map((d,i)=>d+':'+(car.door[i]?'OPEN':'CLOSED')).join(' '),
        windows:DOORS.map((d,i)=>`${d}:${posName(car.win[i])}(${car.win[i]}%)`).join(' '),
        easyEntry:DOORS.map((d,i)=>d+':'+st.entry[i]).join(' '),
        mirrors:`L:${car.mirrors} R:${car.mirrors}`,mirrorFoldedByFeature:st.foldedByUs,camera:car.camera?'ON':'OFF',
        hazard:car.hazard,hazardOwned:st.hazardOwned,pedalPct:car.pedal,brake:car.brake?'ON':'OFF',kickdown:!cfg.kdEnabled?'OFF':kd.kicked?'KICKED':'ARMED',handles:`DRIVER:${car.handle[0]?'PULLED':'-'} PASSENGER:${car.handle[1]?'PULLED':'-'}`,lock:car.locked?'LOCKED':'UNLOCKED',listenOnly:car.listenOnly,blinkActive:Date.now()<st.blinkUntil,reverseHazard:cfg.rvHazard&&car.gear==='R',doorHazard:st.doorHazActive,rearLight:st.rearLight},
      stats:{...stats,lastMacroButton:lastMacro.button,lastMacroGesture:lastMacro.gesture,lastMacroAction:lastMacro.action,lastMacroResult:lastMacro.result}};
  }
  window.__bcPreview={car,cfg,st,stats,wifi,doorOpened,doorClosed,runAction,log,
    macro(button,gesture){stats.macroEvents++;lastMacro={button,gesture,action:cfg.mcAction[button][gesture],result:'OK'};if(!cfg.mcEnabled){log('Macro ignored (macros OFF)');return}const a=cfg.mcAction[button][gesture];if(!a){log('Macro slot empty');return}const r=runAction(a);lastMacro.result=r||'OK';if(r)log('Macro rejected: '+r)},
    userHazard(){if(car.hazard==='ON'){car.hazard='OFF';if(st.hazardOwned){st.hazardOwned=false;st.userHazard=true}}else{car.hazard='ON';st.hazardOwned=false}log('Driver pressed hazard button')}};
  window.fetch=async(input,init={})=>{
    const u=new URL(typeof input==='string'?input:input.url,location.href);
    if(u.pathname==='/api/body/config')return json(body());
    if(u.pathname==='/api/body/update'){
      let any=false;
      for(const[k,v]of u.searchParams){
        if(k==='mc'){const[b,g,a]=v.split(',').map(Number);cfg.mcAction[b][g]=a;any=true;continue}
        if(typeof cfg[k]==='boolean'){if(!['0','1'].includes(v))return json({ok:false,error:'invalid '+k},400);cfg[k]=v==='1';any=true;continue}
        if(k in ranges){const n=Number(v);if(!Number.isInteger(n)||n<ranges[k][0]||n>ranges[k][1])return json({ok:false,error:'invalid '+k},400);cfg[k]=n;any=true;continue}
        return json({ok:false,error:'invalid '+k},400);
      }
      if(!any)return json({ok:false,error:'no settings'},400);
      tick();return json(body());
    }
    if(u.pathname==='/api/body/action'){const r=runAction(Number(u.searchParams.get('id')));return r?json({ok:false,error:r},409):json(body())}
    if(u.pathname==='/api/system/listen-only'){if((init.method||'GET')==='POST'){car.listenOnly=u.searchParams.get('enabled')==='1';log('Listen-only → '+(car.listenOnly?'ON':'OFF'))}return json({ok:true,listenOnly:car.listenOnly,transitionHeld:false})}
    if(u.pathname==='/api/wifi/timing'){
      if((init.method||'GET')==='POST')for(const[k,v]of u.searchParams)wifi[k]=k.endsWith('Enabled')?v==='1':Number(v);
      wifi.apUptimeS=Math.floor((Date.now()-wifiStart)/1000);
      wifi.hiddenNow=wifi.hideEnabled&&wifi.apUptimeS>=wifi.hideDelayS;
      return json(wifi);
    }
    if(u.pathname==='/api/profile/status'){const r=await base(input,init);const p=await r.json();return json({...p,profile:2,topology:2,turn:1,topologyName:'Body + Chassis',canA:'BODY',canB:'CHASSIS'})}
    return base(input,init);
  };
})();
