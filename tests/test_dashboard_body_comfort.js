// Browser regression for the Body Comfort panel and Wi-Fi timing card.
// Runs against dashboard_source.html, the embedded index_html.h bytes and the
// zh-CN source with a mocked firmware API.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {launchBrowser,dashboardArtifacts}=require('./browser_test_runtime');
const root=path.resolve(__dirname,'..');

function firmware(){
  const cfg={eeEnabled:false,eeDoorMask:15,eeWeatherBlock:true,eeWeatherStrict:false,eeRestoreDelayMs:800,eeMirrorFold:false,eeMirrorCamera:true,eeUnfoldKph:10,eeUnfoldHoldS:3,sbEnabled:false,sbSeatMask:7,mcEnabled:false,mcLongMs:800,mcGapMs:400,mcAction:[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],hzBlinkEnabled:false,hzBlinkSeconds:3,rvHazard:false,rvLight:false,rvLightMode:2,drHazard:false,drOffDelayMs:2000,kdEnabled:false,kdTriggerPct:90,kdTargetRaw:1,kdReleaseOnPedal:true,kdReleasePct:20,kdReleaseOnBrake:true,kdReleaseMode:0,dhEnabled:false,dhDoorMask:1,dhHoldS:2,dhAction:20};
  const wifi={ok:true,slowStartEnabled:false,startDelayS:30,hideEnabled:false,hideDelayS:300,hiddenNow:false,startDelayAppliedS:0,apUptimeS:12,setupMode:false};
  const ranges={eeDoorMask:[0,15],eeRestoreDelayMs:[0,5000],eeUnfoldKph:[1,80],eeUnfoldHoldS:[0,30],sbSeatMask:[0,7],mcLongMs:[500,2000],mcGapMs:[200,800],hzBlinkSeconds:[1,30],rvLightMode:[0,2],drOffDelayMs:[0,10000],kdTriggerPct:[10,100],kdTargetRaw:[1,2],kdReleasePct:[0,90],kdReleaseMode:[0,1],dhDoorMask:[0,3],dhHoldS:[1,10],dhAction:[0,22]};
  const state={cfg,wifi,supported:true,failNext:0,actions:[]};
  state.json=()=>({ok:true,supported:state.supported,bus:state.supported?'BODY_CAN_A':'NONE',layout:state.supported?'JUNIPER_VEH':'NONE',taskRunning:true,cfg:JSON.parse(JSON.stringify(cfg)),live:{gear:'P',speedKph:0,weather:'CLEAR',doors:'FL:CLOSED FR:CLOSED RL:CLOSED RR:CLOSED',windows:'FL:CLOSED FR:CLOSED RL:CLOSED RR:CLOSED',easyEntry:'FL:IDLE FR:IDLE RL:IDLE RR:IDLE',mirrors:'L:UNFOLDED R:UNFOLDED',mirrorFoldedByFeature:false,camera:'OFF',hazard:'OFF',hazardOwned:false,blinkActive:false,reverseHazard:false,doorHazard:false,rearLight:false},stats:{txOk:4,txFail:0,entryOpens:1,entryRestores:1,entryBlockedWeather:0,seatbeltInjected:0,macroEvents:0}});
  state.update=params=>{
    if(!state.supported)return[409,{ok:false,error:'unsupported'}];
    if(state.failNext>0){state.failNext--;return[500,{ok:false,error:'Settings could not be saved'}]}
    let any=false;
    for(const[k,v]of params){
      if(k==='mc'){const[b,g,a]=v.split(',').map(Number);cfg.mcAction[b][g]=a;any=true;continue}
      if(typeof cfg[k]==='boolean'){if(!['0','1'].includes(v))return[400,{ok:false,error:'invalid '+k}];cfg[k]=v==='1';any=true;continue}
      if(k in ranges){const n=Number(v);if(!Number.isInteger(n)||n<ranges[k][0]||n>ranges[k][1])return[400,{ok:false,error:'invalid '+k}];cfg[k]=n;any=true;continue}
      return[400,{ok:false,error:'unknown '+k}];
    }
    return any?[200,state.json()]:[400,{ok:false,error:'no settings'}];
  };
  return state;
}

async function check(html,label,zh=false){
  const browser=await launchBrowser();
  try{
    const page=await browser.newPage({viewport:{width:390,height:844}}),requests=[],fw=firmware();
    page.on('pageerror',e=>{throw e});
    await page.route('http://body.test/**',async route=>{
      const request=route.request(),url=new URL(request.url());
      requests.push(`${request.method()} ${url.pathname}${url.search}`);
      if(url.pathname==='/')return route.fulfill({contentType:'text/html',body:html});
      let status=200,body={};
      if(url.pathname==='/api/profile/status')body={setupMode:false,profile:2,topology:2,turn:1,nagSupported:true,nagTorqueSupported:false,nagTsl9Supported:true,advancedEapSupported:true,euUnlockSupported:true};
      else if(url.pathname==='/api/features/status')body={lab:false,s3xy:false,doorCancel:false};
      else if(url.pathname==='/api/body/config')body=fw.json();
      else if(url.pathname==='/api/body/update')[status,body]=fw.update([...url.searchParams]);
      else if(url.pathname==='/api/system/listen-only'){
        if(request.method()==='POST'){if(fw.failListen){status=503;body={ok:false,error:'CAN mode change not confirmed'}}else{fw.listenOnly=url.searchParams.get('enabled')==='1';body={ok:true,listenOnly:fw.listenOnly}}}
        else body={ok:true,listenOnly:!!fw.listenOnly};
      }
      else if(url.pathname==='/api/body/action'){fw.actions.push(url.searchParams.get('id'));body=fw.json()}
      else if(url.pathname==='/api/wifi/timing'){
        if(request.method()==='POST'){for(const[k,v]of url.searchParams)fw.wifi[k]=k.endsWith('Enabled')?v==='1':Number(v)}
        body=fw.wifi;
      }
      await route.fulfill({status,contentType:'application/json',body:JSON.stringify(body)});
    });
    await page.goto('http://body.test/',{waitUntil:'domcontentloaded'});
    await page.waitForFunction(()=>universalBootReady);
    // Boot may still switch pages once; keep requesting Settings until the row is visible.
    await page.waitForFunction(()=>{const r=document.getElementById('bodyComfortSettingsRow');if(r&&r.offsetParent)return true;showPage('settings');return false},null,{polling:100,timeout:10000});
    const row=page.locator('#bodyComfortSettingsRow');
    assert.equal(await row.count(),1,`${label}: settings row missing`);
    await page.waitForFunction(()=>document.getElementById('bodyComfortRowState')?.textContent!=='—');
    assert.match(await page.locator('#bodyComfortRowState').textContent(),zh?/关/:/OFF/);
    await row.click();
    await page.waitForFunction(()=>document.getElementById('panelBodyComfort')?.classList.contains('show'));
    if(zh)assert.equal(await page.locator('#panelBodyComfort .panelHero').textContent(),'车身舒适功能');
    assert.equal(await page.locator('#panelBodyComfort select[id^="bcM"]').count(),24,'macro selects');
    assert.deepEqual(await page.locator('#panelBodyComfort select[id^="bcM"]').evaluateAll(s=>s.slice(0,6).map(e=>e.id)),['bcM6_0','bcM6_1','bcM6_2','bcM7_0','bcM7_1','bcM7_2'],'voice and camera listed first');
    assert.equal(await page.locator('#bcRlm option').count(),3);
    assert.equal(await page.locator('#bcM0_0 option').count(),22,'None + 11 Body Comfort + 10 existing-feature actions');
    assert.equal(await page.locator('#bcHdA').inputValue(),'20','door handle defaults to Open frunk');
    assert.equal(await page.locator('#bcM0_0 option[value="4"]').count(),0,'retired action hidden');
    // Macro groups start collapsed and say "Not set".
    assert.equal(await page.locator('.bcMgBody:not([hidden])').count(),0,'all macro groups collapsed');
    assert.equal(await page.locator('#bcM6_0').isVisible(),false);
    assert.match(await page.locator('#bcMgS6').textContent(),zh?/未设置/:/Not set/);
    assert.equal(await page.locator('#bcMgN6').isHidden(),true);
    assert.equal(await page.locator('#bcSupport').isHidden(),true,'Juniper layout note hidden');
    assert.equal(await page.locator('#bcEe').isChecked(),false,'all features start OFF');

    // Toggle Easy Entry.
    await page.locator('#bcEe').locator('xpath=..').click();
    await page.waitForFunction(()=>/^1 /.test(document.getElementById('bodyComfortRowState').textContent));
    assert.equal(await page.locator('#bcEe').isChecked(),true);
    assert(requests.includes('POST /api/body/update?eeEnabled=1'),requests.join('\n'));
    assert.match(await page.locator('#bodyComfortRowState').textContent(),zh?/1 开/:/1 ON/);
    // Door mask: front-right off.
    await page.locator('#bcDoor1').locator('xpath=..').click();
    await page.waitForFunction(()=>!document.getElementById('bcDoor1').checked);
    await page.waitForTimeout(80);
    assert(requests.includes('POST /api/body/update?eeDoorMask=13'));
    assert.equal(fw.cfg.eeDoorMask,13);
    // Macro slot: right scroll long press -> Blink hazards.
    await page.evaluate(()=>{const s=document.getElementById('bcM1_2');s.value='8';s.dispatchEvent(new Event('change',{bubbles:true}))});
    await page.waitForTimeout(50);
    assert(requests.includes('POST /api/body/update?mc=1%2C2%2C8'),requests.join('\n'));
    assert.equal(fw.cfg.mcAction[1][2],8);
    await page.evaluate(()=>{const s=document.getElementById('bcM7_0');s.value='9';s.dispatchEvent(new Event('change',{bubbles:true}))});
    await page.waitForTimeout(50);
    assert.equal(fw.cfg.mcAction[7][0],9,'camera double -> fold mirrors');
    // Existing feature: voice long -> Nag Killer, via the collapsed group.
    await page.locator('#bcMg6 .bcMgHead').click();
    assert.equal(await page.locator('#bcMg6 .bcMgHead').getAttribute('aria-expanded'),'true');
    assert.equal(await page.locator('#bcM6_2').isVisible(),true);
    await page.evaluate(()=>{const s=document.getElementById('bcM6_2');s.value='11';s.dispatchEvent(new Event('change',{bubbles:true}))});
    await page.waitForFunction(()=>document.getElementById('bcMgN6').textContent==='1');
    assert.equal(fw.cfg.mcAction[6][2],11);
    await page.locator('#bcMg6 .bcMgHead').click();
    assert.equal(await page.locator('#bcM6_2').isVisible(),false,'collapses again');
    assert.match(await page.locator('#bcMgS6').textContent(),zh?/长按 → 切换 Nag Killer/:/Long → Toggle Nag Killer/);
    assert.equal(await page.locator('#bcMg6').evaluate(e=>e.classList.contains('set')),true);
    assert.match(await page.locator('#bcMgS7').textContent(),zh?/双击 → 折叠/:/Double → Fold \/ unfold mirrors/);
    assert.match(await page.locator('#bcMgS1').textContent(),zh?/长按 → 双闪闪烁/:/Long → Hazard blink/);
    assert.match(await page.locator('#bcMcLabel').textContent(),zh?/已设置 3 个/:/3 set/);
    // Kickdown: target select, numeric save with trigger/release validation.
    await page.evaluate(()=>{const s=document.getElementById('bcKdTgt');s.value='1';s.dispatchEvent(new Event('change',{bubbles:true}))});
    await page.waitForTimeout(50);
    assert.equal(fw.cfg.kdTargetRaw,2,'Sport');
    await page.locator('#bcKd').locator('xpath=..').click();
    await page.waitForTimeout(80);
    assert.equal(fw.cfg.kdEnabled,true);
    await page.locator('#bcKdT').fill('70');
    await page.locator('#bcKdR').fill('15');
    await page.locator('#bcKdSave').click();
    await page.waitForFunction(()=>!document.getElementById('bcKdT').dataset.dirty);
    assert.equal(fw.cfg.kdTriggerPct,70);assert.equal(fw.cfg.kdReleasePct,15);
    await page.evaluate(()=>{const s=document.getElementById('bcKdRm');s.value='1';s.dispatchEvent(new Event('change',{bubbles:true}))});
    await page.waitForTimeout(50);
    assert.equal(fw.cfg.kdReleaseMode,1);
    // Door handle macro: passenger added, hold 3 s, action TLSSC.
    await page.locator('#bcHd1').locator('xpath=..').click();
    await page.waitForTimeout(80);
    assert.equal(fw.cfg.dhDoorMask,3);
    await page.locator('#bcHdS').fill('3');
    await page.locator('#bcHdSave').click();
    await page.waitForTimeout(80);
    assert.equal(fw.cfg.dhHoldS,3);
    await page.evaluate(()=>{const s=document.getElementById('bcHdA');s.value='15';s.dispatchEvent(new Event('change',{bubbles:true}))});
    await page.waitForTimeout(50);
    assert.equal(fw.cfg.dhAction,15);
    // Hazard blink is described in seconds.
    assert.match(await page.locator('#bcHbLabel').textContent(),zh?/持续 3 秒/:/for 3 seconds/);
    // Numeric save with validation.
    await page.locator('#bcRd').fill('6000');
    await page.locator('#bcEeSave').click();
    await page.waitForTimeout(50);
    assert.equal(requests.some(r=>r.includes('eeRestoreDelayMs=6000')),false,'out-of-range value must not be posted');
    await page.locator('#bcRd').fill('1500');
    await page.locator('#bcUk').fill('20');
    await page.locator('#bcEeSave').click();
    await page.waitForFunction(()=>document.getElementById('bcSaveState').textContent.length>0&&!document.getElementById('bcRd').dataset.dirty);
    assert(requests.includes('POST /api/body/update?eeRestoreDelayMs=1500&eeUnfoldKph=20&eeUnfoldHoldS=3'),requests.join('\n'));
    assert.equal(fw.cfg.eeRestoreDelayMs,1500);
    // Polling never overwrites a field being edited.
    await page.locator('#bcDd').fill('4500');
    await page.waitForTimeout(1300);
    assert.equal(await page.locator('#bcDd').inputValue(),'4500');
    await page.locator('#bcDhSave').click();
    await page.waitForFunction(()=>!document.getElementById('bcDd').dataset.dirty);
    assert.equal(fw.cfg.drOffDelayMs,4500);
    // Failed save reverts the toggle.
    fw.failNext=1;
    await page.locator('#bcRh').locator('xpath=..').click();
    await page.waitForTimeout(150);
    assert.equal(await page.locator('#bcRh').isChecked(),false,'failed save must revert');
    assert.equal(fw.cfg.rvHazard,false);
    // Rear-light mode and action button.
    await page.evaluate(()=>{const s=document.getElementById('bcRlm');s.value='0';s.dispatchEvent(new Event('change',{bubbles:true}))});
    await page.waitForTimeout(50);
    assert.equal(fw.cfg.rvLightMode,0);
    await page.locator('#bcFoldNow').click();
    await page.waitForTimeout(50);
    assert.deepEqual(fw.actions,['9']);
    // Layout: no horizontal overflow, controls stay on screen.
    for(const width of[320,390,430])for(const theme of['light','dark']){
      await page.setViewportSize({width,height:844});await page.evaluate(t=>window.t2Theme.setMode(t),theme);
      const fits=await page.locator('#panelBodyComfort').evaluate(p=>p.scrollWidth<=innerWidth&&[...p.querySelectorAll('.toggle,select,button.btn,input')].every(e=>{const r=e.getBoundingClientRect();return r.width===0||(r.left>=-1&&r.right<=innerWidth+1)}));
      assert.equal(fits,true,`${label} ${width}/${theme} overflow`);
      const dir=process.env.DASHBOARD_SCREENSHOT_DIR;if(dir){fs.mkdirSync(dir,{recursive:true});await page.screenshot({path:path.join(dir,`${label}-body-comfort-${width}-${theme}.png`),animations:'disabled'});}
    }
    // Unsupported topology: controls disabled with an explanation.
    fw.supported=false;
    await page.waitForFunction(()=>document.getElementById('bcEe').disabled,null,{timeout:3000});
    assert.equal(await page.locator('#bcSupport').isVisible(),true);
    fw.supported=true;

    // Listen-only toggle in Settings > System.
    await page.evaluate(()=>closePanels('back'));
    const loRow=page.locator('#listenOnlyRow');
    assert.equal(await loRow.count(),1);
    assert.equal(await page.locator('#listenOnlyToggle').isChecked(),false);
    await loRow.locator('label.toggle').click();
    await page.waitForTimeout(150);
    assert.equal(fw.listenOnly,true);
    assert.equal(await page.locator('#listenOnlyToggle').isChecked(),true);
    fw.failListen=true;
    await loRow.locator('label.toggle').click();
    await page.waitForTimeout(150);
    assert.equal(await page.locator('#listenOnlyToggle').isChecked(),true,'failed switch reverts the toggle');
    assert.ok((await page.locator('#listenOnlyState').textContent()).length>0,'failure explained');
    fw.failListen=false;
    await loRow.locator('label.toggle').click();
    await page.waitForTimeout(150);
    assert.equal(fw.listenOnly,false);
    // Wi-Fi timing in the Wireless panel.
    await page.locator('[data-panel="panelWireless"]').click();
    await page.waitForFunction(()=>document.getElementById('wifiStartDelay').value==='30',null,{timeout:5000});
    await page.locator('#wifiSlowToggle').locator('xpath=..').click();
    await page.waitForFunction(()=>document.getElementById('wifiSlowToggle').checked);
    assert(requests.includes('POST /api/wifi/timing?slowStartEnabled=1'));
    await page.locator('#wifiStartDelay').fill('45');
    await page.locator('#wifiHideDelay').fill('120');
    await page.locator('#wifiHideToggle').evaluate(e=>{e.checked=true});
    await page.locator('#wifiTimingApply').click();
    await page.waitForTimeout(100);
    assert(requests.includes('POST /api/wifi/timing?slowStartEnabled=1&startDelayS=45&hideEnabled=1&hideDelayS=120'),requests.join('\n'));
    assert.equal(fw.wifi.startDelayS,45);
    assert.equal(fw.wifi.hideDelayS,120);
    await page.locator('#wifiStartDelay').fill('700');
    const before=requests.length;
    await page.locator('#wifiTimingApply').click();
    await page.waitForTimeout(100);
    assert.equal(requests.slice(before).some(r=>r.startsWith('POST /api/wifi/timing')),false,'invalid delay must not be posted');
    const dir=process.env.DASHBOARD_SCREENSHOT_DIR;if(dir)await page.screenshot({path:path.join(dir,`${label}-wifi-timing.png`),animations:'disabled'});
  }finally{await browser.close();}
}

(async()=>{
  for(const {label,html}of dashboardArtifacts(root))await check(html,label);
  await check(fs.readFileSync(path.join(root,'zh-CN','dashboard_source_zh-CN.html'),'utf8'),'zh-CN',true);
})().then(()=>console.log('PASS Body Comfort panel and Wi-Fi timing (source, embedded, zh-CN)'),e=>{console.error(e);process.exitCode=1;});
