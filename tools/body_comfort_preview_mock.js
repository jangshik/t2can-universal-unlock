// Offline preview mock for the Body Comfort panel and Wi-Fi timing card.
// Simulates a HW4 Model Y Juniper (Body + Chassis) and mirrors the firmware
// behaviour of body_comfort_runtime.h closely enough to try every option.
(()=>{
  const base=window.fetch;
  const json=(v,status=200)=>Promise.resolve(new Response(JSON.stringify(v),{status,headers:{'Content-Type':'application/json'}}));
  const cfg={eeEnabled:false,eeDoorMask:15,eeWeatherBlock:true,eeWeatherStrict:false,eeRestoreDelayMs:800,eeMirrorFold:false,eeMirrorCamera:true,eeUnfoldKph:10,eeUnfoldHoldS:3,sbEnabled:false,sbSeatMask:7,mcEnabled:false,mcLongMs:800,mcGapMs:400,mcAction:[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],hzBlinkEnabled:false,hzBlinkCount:3,rvHazard:false,rvLight:false,rvLightMode:2,drHazard:false,drOffDelayMs:2000};
  const ranges={eeDoorMask:[0,15],eeRestoreDelayMs:[0,5000],eeUnfoldKph:[1,80],eeUnfoldHoldS:[0,30],sbSeatMask:[0,7],mcLongMs:[500,2000],mcGapMs:[200,800],hzBlinkCount:[1,10],rvLightMode:[0,2],drOffDelayMs:[0,10000]};
  const wifi={ok:true,slowStartEnabled:false,startDelayS:30,hideEnabled:false,hideDelayS:300,hiddenNow:false,startDelayAppliedS:0,apUptimeS:0,setupMode:false};
  const wifiStart=Date.now();
  const DOORS=['FL','FR','RL','RR'];
  const car={gear:'P',kph:0,door:[false,false,false,false],win:[0,0,0,0],rain:false,snow:false,mirrors:'UNFOLDED',camera:false,hazard:'OFF',belts:[false,false,false],log:[]};
  const st={entry:['IDLE','IDLE','IDLE','IDLE'],saved:[0,0,0,0],foldedByUs:false,cameraByUs:false,unfoldSince:0,hazardOwned:false,userHazard:false,blinkUntil:0,doorHazUntil:0,doorHazActive:false,rearLight:false};
  const stats={txOk:0,txFail:0,entryOpens:0,entryRestores:0,entryBlockedWeather:0,entryBlockedGear:0,seatbeltInjected:0,macroEvents:0,hazardPresses:0,blinkRuns:0,mirrorFolds:0,mirrorUnfolds:0,cameraPresses:0};
  const log=msg=>{car.log.unshift(new Date().toLocaleTimeString()+'  '+msg);car.log.length=Math.min(car.log.length,12);window.__bcPreviewRender?.()};
  const tx=(what)=>{stats.txOk++;log('TX '+what)};
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
  function tick(){
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
    const toggles={1:'eeEnabled',2:'eeMirrorFold',3:'sbEnabled',4:'hzBlinkEnabled',5:'rvHazard',6:'rvLight',7:'drHazard'};
    if(toggles[a]){cfg[toggles[a]]=!cfg[toggles[a]];log(`Macro: ${toggles[a]} → ${cfg[toggles[a]]?'ON':'OFF'}`);return null}
    if(a===8){if(!cfg.hzBlinkEnabled)return'hazard blink disabled';if(car.hazard!=='OFF')return'hazards already on';st.blinkUntil=Date.now()+cfg.hzBlinkCount*700;stats.blinkRuns++;log(`Hazard blink ×${cfg.hzBlinkCount}`);return null}
    if(a===9){if(car.mirrors==='FOLDED'){car.mirrors='UNFOLDED';st.foldedByUs=false;tx('0x273 UI_mirrorFoldRequest=PRESENT')}else{car.mirrors='FOLDED';st.foldedByUs=true;tx('0x273 UI_mirrorFoldRequest=RETRACT')}return null}
    if(a===10){car.camera=!car.camera;stats.cameraPresses++;tx('0x3C2 mux1 camera button press');return null}
    return'no action';
  }
  function body(){
    return{ok:true,supported:true,bus:'BODY_CAN_A',layout:'JUNIPER_VEH',taskRunning:true,cfg:JSON.parse(JSON.stringify(cfg)),
      live:{gear:car.gear,speedKph:car.kph,weather:car.rain&&car.snow?'RAIN+SNOW':car.rain?'RAIN':car.snow?'SNOW':'CLEAR',
        doors:DOORS.map((d,i)=>d+':'+(car.door[i]?'OPEN':'CLOSED')).join(' '),
        windows:DOORS.map((d,i)=>`${d}:${posName(car.win[i])}(${car.win[i]}%)`).join(' '),
        easyEntry:DOORS.map((d,i)=>d+':'+st.entry[i]).join(' '),
        mirrors:`L:${car.mirrors} R:${car.mirrors}`,mirrorFoldedByFeature:st.foldedByUs,camera:car.camera?'ON':'OFF',
        hazard:car.hazard,hazardOwned:st.hazardOwned,blinkActive:Date.now()<st.blinkUntil,reverseHazard:cfg.rvHazard&&car.gear==='R',doorHazard:st.doorHazActive,rearLight:st.rearLight},
      stats:{...stats}};
  }
  window.__bcPreview={car,cfg,st,stats,wifi,doorOpened,doorClosed,runAction,log,
    macro(button,gesture){stats.macroEvents++;if(!cfg.mcEnabled){log('Macro ignored (macros OFF)');return}const a=cfg.mcAction[button][gesture];if(!a){log('Macro slot empty');return}const r=runAction(a);if(r)log('Macro rejected: '+r)},
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
