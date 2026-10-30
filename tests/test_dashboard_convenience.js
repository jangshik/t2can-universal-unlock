const assert=require('node:assert/strict');
const path=require('node:path');
const {launchBrowser,dashboardArtifacts}=require('./browser_test_runtime');
const root=path.resolve(__dirname,'..');
async function check(html,label){
 const browser=await launchBrowser();
 try{
  const page=await browser.newPage({viewport:{width:390,height:844}}),errors=[],posts=[];
  page.on('pageerror',error=>errors.push(error.message));
  let fail=false,body=true,testFail=false,testPosts=0,hazardPosts=0,hazardFail=false;
  const lab={ready:true,sent:0,lastTx:"00 00 00 00 00 50 11 00",states:Array.from({length:4},()=>({door:1,window:1,doorAgeMs:20,windowAgeMs:20}))};
  const actions=[{id:0,key:'none',label:'None',supported:true},{id:14,key:'master_toggle',label:'CAN Master / Listen Only',supported:true},{id:15,key:'kickdown_toggle',label:'Kickdown',supported:true}];
  actions.push({id:54,key:'hazard_three_flash',label:'Hazard Light · 3 Flashes',supported:true});
  const config={hazardThreeFlashReady:true,hazardThreeFlashActive:false,hazardThreeFlashCount:0,supported:true,macroSupported:true,easyEntrySupported:true,seatbeltTxSupported:true,masterEnabled:true,kickdown:false,easyEntry:false,mirrorSync:false,rearBeltLeft:false,rearBeltCenter:false,rearBeltRight:false,reverseHazard:false,reverseFog:false,macrosEnabled:false,kickOnPct:90,kickOffPct:10,kickReleaseMode:2,debounceMs:30,clickGapMs:350,longPressMs:800,slots:Array.from({length:3},()=>({button:0,gesture:2,action:0})),actions};
  const ap={enabled:false,method:0,supported:true,stalkSupported:false,scrollSingleSupported:true,scrollDoubleSupported:true};
  await page.route('http://convenience.test/**',async route=>{
   const req=route.request(),u=new URL(req.url());let result={};
   if(u.pathname==='/')return route.fulfill({contentType:'text/html',body:html});
   if(u.pathname==='/api/profile/status')result={setupMode:false,profile:2,topology:body?2:3,turn:1,nagSupported:true,nagTorqueSupported:true,nagTsl9Supported:true,advancedEapSupported:true,euUnlockSupported:true};
   else if(u.pathname==='/api/features/status')result={lab:true,s3xy:true,doorCancel:false};
   else if(u.pathname==='/api/lab/easy-entry/status')result=lab;
   else if(u.pathname==='/api/lab/easy-entry/test'){assert.equal(req.method(),'POST');testPosts++;if(testFail)return route.fulfill({status:409,contentType:'application/json',body:JSON.stringify({error:'Park required'})});assert.equal(u.searchParams.get('percent'),'100');lab.sent++;result=lab;}
   else if(u.pathname==='/api/convenience/hazard-three-flash'){assert.equal(req.method(),'POST');hazardPosts++;if(hazardFail)return route.fulfill({status:409,contentType:'application/json',body:'{}'});config.hazardThreeFlashReady=false;config.hazardThreeFlashActive=true;result=config;}
   else if(u.pathname==='/api/feature-actions')result={actions};
   else if(u.pathname==='/api/convenience/config'){
    if(req.method()==='POST'){
     posts.push(u.search);
     if(fail){fail=false;return route.fulfill({status:503,contentType:'application/json',body:'{"error":"save failed"}'});}
     for(const [key,value]of u.searchParams){const slot=key.match(/^slot(\d)(Button|Gesture|Action)$/);if(slot)config.slots[+slot[1]][slot[2].toLowerCase()]=+value;else config[key]=typeof config[key]==='boolean'?value==='1':+value;}
    }
    result={...config,supported:body,kickdownSupported:true,macroSupported:body,seatbeltTxSupported:body};
   }else if(u.pathname==='/api/continuous-ap/config'){
    if(req.method()==='POST'){posts.push(u.search);for(const [k,v]of u.searchParams)ap[k]=typeof ap[k]==='boolean'?v==='1':+v;}
    result=ap;
   }else if(u.pathname==='/api/driver-monitoring/config'||u.pathname==='/api/isa-suppression/config')result={enabled:false,supported:true};
   await route.fulfill({contentType:'application/json',body:JSON.stringify(result)});
  });
  await page.goto('http://convenience.test/',{waitUntil:'domcontentloaded'});
  await page.waitForFunction(()=>universalBootReady);
  await page.getByRole('button',{name:'Settings',exact:true}).click();
  await page.locator('#convenienceSettingsRow').click();
  await page.waitForFunction(()=>!document.querySelector('#conv_kickdown').disabled);
  assert(!await page.locator('#conv_easyEntry').isDisabled());
  assert(!await page.locator('#conv_easyEntry').isChecked());
  await page.locator('label:has(#conv_easyEntry)').click();
  await page.waitForFunction(()=>convenienceControl.saved.easyEntry===true&&!convenienceControl.saving);
  await page.locator('label:has(#conv_easyEntry)').click();
  await page.waitForFunction(()=>convenienceControl.saved.easyEntry===false&&!convenienceControl.saving);
  await page.locator('label:has(#conv_kickdown)').click();
  await page.waitForFunction(()=>convenienceControl.saved.kickdown===true&&!convenienceControl.saving);
  await page.locator('#conv_kickOnPct').fill('75');await page.locator('#conv_kickOnPct').dispatchEvent('change');
  await page.waitForFunction(()=>convenienceControl.saved.kickOnPct===75&&!convenienceControl.saving);
  await page.locator('#conv_slot0Button').selectOption('8');
  await page.waitForFunction(()=>convenienceControl.saved.slots[0].button===8&&!convenienceControl.saving);
  await page.locator('#conv_slot0Gesture').selectOption('3');
  await page.waitForFunction(()=>convenienceControl.saved.slots[0].gesture===3&&!convenienceControl.saving);
  await page.locator('#conv_slot0Action').selectOption('14');
  await page.waitForFunction(()=>convenienceControl.saved.slots[0].action===14&&!convenienceControl.saving);
  await page.locator('#conv_slot0Action').selectOption('54');
  await page.waitForFunction(()=>convenienceControl.saved.slots[0].action===54&&!convenienceControl.saving);
  await page.locator('#hazardThreeFlashSend').click();
  await page.waitForFunction(()=>!hazardThreeFlashSending&&convenienceControl.saved.hazardThreeFlashActive);
  assert.equal(hazardPosts,1);assert(await page.locator('#hazardThreeFlashSend').isDisabled());
  config.hazardThreeFlashActive=false;config.hazardThreeFlashReady=true;await page.evaluate(()=>fetchConvenience());hazardFail=true;
  await page.locator('#hazardThreeFlashSend').click();await page.waitForFunction(()=>document.querySelector('#hazardThreeFlashState').textContent.includes('no automatic retry'));
  assert.equal(hazardPosts,2);
  fail=true;await page.locator('label:has(#conv_reverseFog)').click();
  await page.waitForFunction(()=>!convenienceControl.saving&&convenienceControl.saved.reverseFog===false);
  assert(!config.reverseFog);assert(posts.some(p=>p.includes('reverseFog=1')));
  body=false;await page.evaluate(()=>fetchConvenience());
  assert(!await page.locator('#conv_kickdown').isDisabled());
  assert(!await page.locator('#conv_kickOnPct').isDisabled());
  assert(await page.locator('#conv_macrosEnabled').isDisabled());
  assert(!await page.locator('#conv_masterEnabled').isDisabled());
  body=true;await page.evaluate(()=>fetchConvenience());
  assert.equal(await page.locator('#continuousApAlgorithm').count(),0);
  assert.equal(await page.locator('#entryTestSend').count(),1);
  await page.evaluate(()=>openPanel('panelLabEasyEntry'));
  await page.waitForFunction(()=>!document.querySelector('#entryTestSend').disabled);
  await page.locator('#entryTestSend').click();
  await page.waitForFunction(()=>document.querySelector('#entryTestResult').textContent.includes('Sent once'));assert.equal(testPosts,1);
  testFail=true;await page.locator('#entryTestSend').click();
  await page.waitForFunction(()=>document.querySelector('#entryTestResult').textContent.includes('No automatic retry'));assert.equal(testPosts,2);
  lab.ready=false;await page.evaluate(()=>fetchEntryTest());assert(await page.locator('#entryTestSend').isDisabled());
  assert.deepEqual(errors,[]);
  console.log('PASS convenience/shared actions '+label);
 }finally{await browser.close();}
}
(async()=>{for(const a of dashboardArtifacts(root))await check(a.html,a.label);})().catch(e=>{console.error(e);process.exitCode=1;});
