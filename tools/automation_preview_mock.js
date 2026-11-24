// Offline preview mock for the Automation panel. Loaded after
// body_comfort_preview_mock.js; reads and drives its simulated Juniper
// (window.__bcPreview) with a simplified copy of automation_runtime.h.
(()=>{
  const P=window.__bcPreview,car=P.car,cfg=P.cfg,prev=window.fetch;
  const ANY=-32768;
  const json=(v,status=200)=>Promise.resolve(new Response(JSON.stringify(v),{status,headers:{'Content-Type':'application/json'}}));
  const SIG=[[1,'gear',1,1,4],[2,'speed',2,-50,250],[3,'pedal',2,0,100],[4,'brake',0,0,1],[5,'doorFL',0,0,1],[6,'doorFR',0,0,1],[7,'doorRL',0,0,1],[8,'doorRR',0,0,1],[9,'anyDoor',0,0,1],[10,'handleFL',0,0,1],[11,'handleFR',0,0,1],[12,'locked',0,0,1],[13,'winFL',1,0,3],[14,'winFR',1,0,3],[15,'winRL',1,0,3],[16,'winRR',1,0,3],[17,'anyWindowOpen',0,0,1],[18,'rain',0,0,1],[19,'snow',0,0,1],[20,'rainOrSnow',0,0,1],[21,'mirrors',1,0,2],[22,'camera',0,0,1],[23,'hazards',0,0,1],[24,'lightSwitch',1,0,3],[25,'belt2L',0,0,1],[26,'belt2C',0,0,1],[27,'belt2R',0,0,1]];
  const FEAT=['easyEntry','eeMirrorFold','seatbelt','reverseHazard','reverseLight','doorHazard','kickdown','doorHandle','macros','hazardBlink','nagKiller','isa','dms','autoBlinker','tlssc','doorCancel','vision','contAp','preheat','listenOnly'];
  const FEAT_CFG=['eeEnabled','eeMirrorFold','sbEnabled','rvHazard','rvLight','drHazard','kdEnabled','dhEnabled','mcEnabled','hzBlinkEnabled'];
  const signals=[...SIG.map(([id,key,kind,min,max])=>({id,key,kind,min,max})),...FEAT.map((k,i)=>({id:40+i,key:'feature.'+k,kind:0,min:0,max:1})),...Array.from({length:8},(_,i)=>({id:80+i,key:'flag'+(i+1),kind:0,min:0,max:1}))];
  const catalog={ok:true,signals,featureAvailable:FEAT.map(()=>1),driveBlocked:[[2,1],[6,1],[5,255]],ruleMax:16,condMax:4,actMax:3,flagCount:8,nameBytes:31};
  const legacy=new Array(20).fill(false);
  const A={enabled:true,flags:0,rules:new Array(16).fill(null),log:[],actionsRun:0,driveBlocked:0,fogUntil:0};
  const RES={OK:'OK',DRY:'dry-run (not sent)',BLK:'blocked in D',LO:'listen-only (not sent)'};
  const winVal=p=>p<=0?0:p<=10?1:p<100?2:3;
  function snapshot(){const v={};const gear={P:1,R:2,N:3,D:4}[car.gear];
    Object.assign(v,{1:gear,2:car.kph,3:car.pedal,4:car.brake?1:0,9:car.door.some(Boolean)?1:0,10:car.handle[0]?1:0,11:car.handle[1]?1:0,12:car.locked?1:0,
      17:car.win.some(p=>p>0)?1:0,18:car.rain?1:0,19:car.snow?1:0,20:(car.rain||car.snow)?1:0,21:car.mirrors==='FOLDED'?1:0,22:car.camera?1:0,23:car.hazard==='ON'?1:0,24:0});
    car.door.forEach((o,i)=>v[5+i]=o?1:0);car.win.forEach((p,i)=>v[13+i]=winVal(p));car.belts.forEach((u,i)=>v[25+i]=u?1:0);
    FEAT.forEach((_,i)=>v[40+i]=i<FEAT_CFG.length?(cfg[FEAT_CFG[i]]?1:0):i===19?(car.listenOnly?1:0):(legacy[i]?1:0));
    for(let i=0;i<8;i++)v[80+i]=(A.flags>>i)&1;return v}
  const cmp=(v,op,r)=>[v===r,v!==r,v>r,v>=r,v<r,v<=r][op];
  function decode(name,r){let i=0;const d={name,en:!!r[i++],dry:!!r[i++],restart:!!r[i++],cool:r[i++],tt:r[i++],ts:r[i++],to:r[i++],tv:r[i++],tf:r[i++],th:r[i++],cond:[],act:[]};
    for(let c=0;c<4;c++){const s=r[i++],op=r[i++],or=r[i++],v=r[i++];if(s)d.cond.push({s,op,or,v})}for(let a=0;a<3;a++)d.act.push({t:r[i++],a:r[i++],b:r[i++]});return d}
  function condsPass(d,v){if(!d.cond.length)return true;let any=false,grp=true;d.cond.forEach((c,i)=>{if(i&&c.or){any=any||grp;grp=true}grp=grp&&v[c.s]!==undefined&&cmp(v[c.s],c.op,c.v)});return any||grp}
  function logIt(idx,type,a,b,dry,result){A.log.unshift({ms:Date.now(),rule:idx,type,a,b,dry,result});A.log.length=Math.min(A.log.length,20);const R=A.rules[idx];if(R&&type<254)R.last=result}
  function exec(idx,act){const R=A.rules[idx],d=R.d;
    if([[2,1],[6,1],[5,255]].some(([t,p])=>t===act.t&&(p===255||p===act.a))&&car.gear==='D'){A.driveBlocked++;return RES.BLK}
    if(d.dry)return RES.DRY;
    if(car.listenOnly&&[1,2,3,4,5,6,8].includes(act.t))return RES.LO;
    A.actionsRun++;
    switch(act.t){
      case 1:P.st.blinkUntil=Date.now()+act.b*1000;P.log(`Automation: hazard blink ${act.b}s`);return RES.OK;
      case 2:{const fold=act.a===1;if(fold===(car.mirrors==='FOLDED'))return fold?'already folded':'already unfolded';car.mirrors=fold?'FOLDED':'UNFOLDED';P.log('TX 0x273 UI_mirrorFoldRequest='+(fold?'RETRACT':'PRESENT')+' (automation)');return RES.OK}
      case 3:{const p={2:10,3:0,4:100}[act.b];for(let i=0;i<4;i++)if((act.a>>i)&1)car.win[i]=p;P.log(`TX 0x294 windows mask ${act.a} → ${{2:'VENT',3:'CLOSED',4:'OPEN'}[act.b]} (automation)`);return RES.OK}
      case 4:car.camera=!car.camera;P.log('TX 0x3C2 camera button (automation)');return RES.OK;
      case 5:if(car.gear!=='P')return'frunk only opens in P';car.frunk=true;P.log('TX 0x273 frunk open (automation)');return RES.OK;
      case 6:A.fogUntil=act.a?Date.now()+act.b*1000:0;P.log(act.a?`Rear fog ON for ${act.b}s (automation)`:'Rear fog OFF (automation)');return RES.OK;
      case 7:{const next=act.b===2?null:act.b===1;if(act.a<FEAT_CFG.length){const k=FEAT_CFG[act.a];const n=next===null?!cfg[k]:next;if(n===cfg[k])return'already set';cfg[k]=n}
        else if(act.a===19){const n=next===null?!car.listenOnly:next;if(n===car.listenOnly)return'already set';car.listenOnly=n}
        else{const n=next===null?!legacy[act.a]:next;if(n===legacy[act.a])return'already set';legacy[act.a]=n}P.log(`Automation: ${FEAT[act.a]} → ${act.b===2?'toggle':act.b?'ON':'OFF'}`);return RES.OK}
      case 8:car.pedalMode=['Comfort (Chill)','Standard','Sport','Vehicle setting'][act.a];P.log('TX 0x334 pedal map → '+car.pedalMode+' (automation)');return RES.OK;
      case 9:{const bit=1<<act.a;const n=act.b===2?A.flags^bit:act.b?A.flags|bit:A.flags&~bit;if(n===A.flags)return'already set';A.flags=n;return RES.OK}
      default:return'no action'}}
  function step(idx,now){const R=A.rules[idx];while(R.running&&R.step<3&&now>=R.due){const a=R.d.act[R.step];R.step++;if(!a.t)continue;if(a.t===10){R.due=now+a.b*1000;continue}logIt(idx,a.t,a.a,a.b,R.d.dry,exec(idx,a))}if(R.step>=3&&now>=R.due)R.running=false}
  function start(idx,now,manual){const R=A.rules[idx];R.running=true;R.manual=manual;R.step=0;R.due=now;if(!manual){R.fired=true;R.lastMs=now;R.fires++}logIt(idx,manual?254:255,0,0,R.d.dry,manual?'run now':'triggered')}
  function tick(){const now=Date.now(),v=snapshot();
    A.rules.forEach((R,idx)=>{if(!R)return;const d=R.d,val=v[d.ts],t=R.t;let fire=false;
      if(val!==undefined){const was=t.known,prev=t.last;t.known=true;t.last=val;
        if(d.tt===1){const pred=cmp(val,d.to,d.tv);if(!pred)t.armed=false;else if(was&&!cmp(prev,d.to,d.tv)&&!t.armed){t.armed=true;t.armMs=now}}
        else{if(t.armed&&val!==t.armVal)t.armed=false;if(was&&prev!==val&&(d.tf===ANY||d.tf===prev)&&(d.tv===ANY||d.tv===val)){t.armed=true;t.armMs=now;t.armVal=val}}
        if(t.armed&&now-t.armMs>=d.th*1000){t.armed=false;fire=true}}
      if(fire&&A.enabled&&d.en){if(R.fired&&now-R.lastMs<d.cool*1000)R.skippedCooldown++;else if(!condsPass(d,v)){R.skippedCond++;R.last='conditions not met'}else if(R.running&&!d.restart)R.skippedBusy++;else start(idx,now,false)}
      if(R.running&&!R.manual&&(!A.enabled||!d.en))R.running=false;if(R.running)step(idx,now)});
    if(A.fogUntil&&(now>=A.fogUntil||car.gear==='D'))A.fogUntil=0}
  setInterval(tick,250);
  function status(){const now=Date.now();return{ok:true,supported:true,enabled:A.enabled,listenOnly:car.listenOnly,flags:A.flags,ruleMax:16,
    rules:A.rules.map((R,idx)=>R&&{idx,name:R.name,r:R.r,fires:R.fires,lastAgoS:R.fired?Math.floor((now-R.lastMs)/1000):-1,running:R.running,skippedCond:R.skippedCond,skippedCooldown:R.skippedCooldown,skippedBusy:R.skippedBusy,last:R.last||''}).filter(Boolean),
    log:A.log.map(e=>({agoS:Math.floor((now-e.ms)/1000),rule:e.rule,type:e.type,a:e.a,b:e.b,dry:e.dry,result:e.result})),
    stats:{actionsRun:A.actionsRun,driveBlocked:A.driveBlocked,rateLimited:0,saveFailures:0,rearFogActive:A.fogUntil>now,rearFogLeftS:A.fogUntil>now?Math.ceil((A.fogUntil-now)/1000):0}}}
  function put(idx,name,r){A.rules[idx]={name,r,d:decode(name,r),t:{},fires:0,fired:false,lastMs:0,running:false,step:0,due:0,skippedCond:0,skippedCooldown:0,skippedBusy:0,last:''}}
  // Two example rules so the panel is not empty.
  put(0,'Rain: close windows when locked',[1,1,0,60,1,20,0,1,ANY,0,1,0,0,1,12,0,0,1,0,0,0,0,0,0,0,0,3,15,3,0,0,0,0,0,0]);
  put(1,'Car wash mode (Flag 1)',[1,0,0,5,1,80,0,1,ANY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,7,1,0,7,5,0]);
  window.__autoPreview={A,tick};
  window.fetch=async(input,init={})=>{
    const u=new URL(typeof input==='string'?input:input.url,location.href),m=(init.method||'GET').toUpperCase(),q=k=>u.searchParams.get(k);
    if(!u.pathname.startsWith('/api/auto'))return prev(input,init);
    if(u.pathname==='/api/auto')return json(status());
    if(u.pathname==='/api/auto/catalog')return json(catalog);
    if(m!=='POST')return json({ok:false,error:'POST required'},405);
    const idx=Number(q('idx'));
    if(u.pathname==='/api/auto/rule'){const r=(q('r')||'').split(',').map(Number);if(r.length!==35||r.some(x=>!Number.isInteger(x)))return json({ok:false,error:'rule data: 35 fields required'},400);
      if(r[3]<1||r[3]>3600)return json({ok:false,error:'cooldown 1-3600 s'},400);if(!r.slice(26).some((x,i)=>i%3===0&&x&&x!==10))return json({ok:false,error:'at least one action'},400);
      put(idx,(q('name')||'').replace(/["\\\u0000-\u001f]/g,'').slice(0,31),r);tick();return json(status())}
    if(u.pathname==='/api/auto/delete'){A.rules[idx]=null;return json(status())}
    if(u.pathname==='/api/auto/global'){A.enabled=q('enabled')==='1';return json(status())}
    if(u.pathname==='/api/auto/flag'){const b=1<<idx;A.flags=q('value')==='1'?A.flags|b:A.flags&~b;tick();return json(status())}
    if(u.pathname==='/api/auto/run'){const R=A.rules[idx];if(!R)return json({ok:false,error:'empty rule'},409);if(R.running)return json({ok:false,error:'rule is running'},409);start(idx,Date.now(),true);step(idx,Date.now());return json(status())}
    return json({ok:false,error:'unknown'},404);
  };
})();
