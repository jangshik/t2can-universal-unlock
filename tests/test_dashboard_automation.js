// Browser regression for the Automation panel. Runs the dashboard source, the
// embedded index_html.h bytes and the zh-CN source; /automation.js is served
// from the gzip bytes in automation_ui_js.h (and checked against its source).
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),zlib=require('node:zlib');
const {launchBrowser,dashboardArtifacts}=require('./browser_test_runtime');
const root=path.resolve(__dirname,'..');

function automationJs(){
  const header=fs.readFileSync(path.join(root,'automation_ui_js.h'),'utf8');
  const data=header.match(/AUTOMATION_UI_JS_GZ\[\][^{]*\{([\s\S]*?)\};/)[1];
  const js=zlib.gunzipSync(Buffer.from(data.match(/0x[\da-f]+/gi).map(x=>parseInt(x,16)))).toString();
  const src=fs.readFileSync(path.join(root,'dashboard_automation.js'),'utf8');
  const minified=src.split('\n').map(l=>l.trim()).filter(l=>l&&!l.startsWith('//')).join('\n')+'\n';
  assert.equal(js,minified,'automation_ui_js.h is stale relative to dashboard_automation.js');
  return js;
}

function firmware(){
  const SIG=[[1,'gear',1,1,4],[2,'speed',2,-50,250],[3,'pedal',2,0,100],[4,'brake',0,0,1],[5,'doorFL',0,0,1],[6,'doorFR',0,0,1],[7,'doorRL',0,0,1],[8,'doorRR',0,0,1],[9,'anyDoor',0,0,1],[10,'handleFL',0,0,1],[11,'handleFR',0,0,1],[12,'locked',0,0,1],[13,'winFL',1,0,3],[14,'winFR',1,0,3],[15,'winRL',1,0,3],[16,'winRR',1,0,3],[17,'anyWindowOpen',0,0,1],[18,'rain',0,0,1],[19,'snow',0,0,1],[20,'rainOrSnow',0,0,1],[21,'mirrors',1,0,2],[22,'camera',0,0,1],[23,'hazards',0,0,1],[24,'lightSwitch',1,0,3],[25,'belt2L',0,0,1],[26,'belt2C',0,0,1],[27,'belt2R',0,0,1]];
  const feats=['easyEntry','eeMirrorFold','seatbelt','reverseHazard','reverseLight','doorHazard','kickdown','doorHandle','macros','hazardBlink','nagKiller','isa','dms','autoBlinker','tlssc','doorCancel','vision','contAp','preheat','listenOnly'];
  const signals=[...SIG.map(([id,key,kind,min,max])=>({id,key,kind,min,max})),...feats.map((k,i)=>({id:40+i,key:'feature.'+k,kind:0,min:0,max:1})),...Array.from({length:8},(_,i)=>({id:80+i,key:'flag'+(i+1),kind:0,min:0,max:1}))];
  const fw={enabled:false,flags:0,rules:{},posts:[],
    catalog:{ok:true,signals,featureAvailable:feats.map((_,i)=>i===16?0:1),driveBlocked:[[2,1],[6,1],[5,255]],ruleMax:16,condMax:4,actMax:3,flagCount:8,nameBytes:31}};
  fw.rules[2]={idx:2,name:'Rain close',r:[1,0,0,60,1,20,0,1,-32768,0,1,0,0,1,12,0,0,1,0,0,0,0,0,0,0,0,3,15,3,0,0,0,0,0,0],fires:3,lastAgoS:75,running:false,skippedCond:1,skippedCooldown:0,skippedBusy:0,last:'OK'};
  fw.json=()=>({ok:true,supported:true,enabled:fw.enabled,listenOnly:false,flags:fw.flags,ruleMax:16,rules:Object.values(fw.rules).sort((a,b)=>a.idx-b.idx),
    log:[{agoS:75,rule:2,type:255,a:0,b:0,dry:false,result:'triggered'},{agoS:75,rule:2,type:3,a:15,b:3,dry:false,result:'OK'}],stats:{actionsRun:3,driveBlocked:1}});
  fw.handle=(method,url)=>{const q=k=>url.searchParams.get(k);
    if(url.pathname==='/api/auto')return[200,fw.json()];
    if(url.pathname==='/api/auto/catalog')return[200,fw.catalog];
    if(method!=='POST')return[405,{ok:false}];
    fw.posts.push(url.pathname+url.search);
    if(url.pathname==='/api/auto/global'){fw.enabled=q('enabled')==='1';return[200,fw.json()]}
    if(url.pathname==='/api/auto/flag'){const b=1<<Number(q('idx'));fw.flags=q('value')==='1'?fw.flags|b:fw.flags&~b;return[200,fw.json()]}
    if(url.pathname==='/api/auto/rule'){const r=q('r').split(',').map(Number);if(r.length!==35)return[400,{ok:false,error:'rule data: 35 fields required'}];
      if(r[3]<1)return[400,{ok:false,error:'cooldown 1-3600 s'}];
      const idx=Number(q('idx'));fw.rules[idx]={idx,name:q('name'),r,fires:0,lastAgoS:-1,running:false,skippedCond:0,skippedCooldown:0,skippedBusy:0,last:''};return[200,fw.json()]}
    if(url.pathname==='/api/auto/delete'){delete fw.rules[Number(q('idx'))];return[200,fw.json()]}
    if(url.pathname==='/api/auto/run')return[200,fw.json()];
    return[404,{ok:false}]};
  return fw;
}

async function check(html,label,js,zh=false){
  const browser=await launchBrowser();
  try{
    for(const width of [390,320]){
    const page=await browser.newPage({viewport:{width,height:844}}),fw=firmware();let jsLoads=0;
    page.on('pageerror',e=>{throw e});
    page.on('dialog',d=>d.accept());
    await page.route('http://auto.test/**',async route=>{
      const request=route.request(),url=new URL(request.url());
      if(url.pathname==='/')return route.fulfill({contentType:'text/html',body:html});
      if(url.pathname==='/automation.js'){jsLoads++;return route.fulfill({contentType:'application/javascript',body:js})}
      let status=200,body={};
      if(url.pathname.startsWith('/api/auto'))[status,body]=fw.handle(request.method(),url);
      else if(url.pathname==='/api/profile/status')body={setupMode:false,profile:2,topology:2,turn:1};
      else if(url.pathname==='/api/features/status')body={lab:false,s3xy:false,doorCancel:false};
      else if(url.pathname==='/api/body/config')body={ok:true,supported:true,cfg:{},live:{},stats:{}};
      await route.fulfill({status,contentType:'application/json',body:JSON.stringify(body)});
    });
    await page.goto('http://auto.test/',{waitUntil:'domcontentloaded'});
    await page.waitForFunction(()=>universalBootReady);
    await page.waitForFunction(()=>{const r=document.getElementById('automationSettingsRow');if(r&&r.offsetParent)return true;showPage('settings');return false},null,{polling:100,timeout:10000});
    await page.waitForFunction(()=>document.getElementById('automationRowState')?.textContent!=='—');
    assert.equal(await page.locator('#automationRowState').textContent(),(zh?'关':'OFF')+' · 1',`${label}: row state`);
    assert.equal(jsLoads,0,'panel script is loaded on demand only');
    await page.locator('#automationSettingsRow').click();
    await page.waitForFunction(()=>document.getElementById('panelAutomation')?.classList.contains('show'));
    await page.waitForSelector('#auRules .auRow');
    assert.equal(jsLoads,1);
    if(zh)assert.equal(await page.locator('#panelAutomation .panelHero').textContent(),'自动化');
    const sum=await page.locator('#auRules .auRow[data-key="r2"] .auSum').textContent();
    assert.equal(sum,zh?'当 雨或雪 = 是 · 如果 挡位 = P 并且 车辆 = 已锁 → 车窗 (全部): 关闭':'WHEN Rain or snow = yes · IF Gear = P AND Car = locked → Windows (all): Close',label);
    assert.match(await page.locator('#auSt2').textContent(),zh?/执行 3 次 · 1分前 · OK/:/3 run\(s\) · 1min ago · OK/);
    assert.match(await page.locator('#auLog').textContent(),zh?/已触发/:/triggered/);
    // Master switch and flags.
    await page.locator('#auMasterCard .toggle').click();
    await page.waitForFunction(()=>document.getElementById('automationRowState').textContent.includes('·')&&document.getElementById('auMaster').checked);
    assert.ok(fw.posts.includes('/api/auto/global?enabled=1'));
    await page.locator('#auFlags [data-flag="2"]').click();
    await page.waitForFunction(()=>document.querySelector('#auFlags [data-flag="2"]').classList.contains('on'));
    assert.ok(fw.posts.includes('/api/auto/flag?idx=2&value=1'));
    // New rule: first free slot is 0, starts in test mode.
    await page.locator('#auAdd').click();
    await page.waitForSelector('#auRules .auRow[data-key="n0"] .auEd');
    assert.equal(await page.locator('#auDry').isChecked(),true,'new rules start in test mode');
    await page.selectOption('#auRules .auEd select[data-k="ts"]','2');
    await page.selectOption('#auRules .auEd select[data-k="to"]','2');
    await page.fill('#auRules .auEd input[data-k="tv"]','100');
    await page.dispatchEvent('#auRules .auEd input[data-k="tv"]','change');
    await page.fill('#auRules .auEd input[data-k="th"]','10');
    await page.dispatchEvent('#auRules .auEd input[data-k="th"]','change');
    await page.locator('#auRules .auEd [data-act="addc"]').click();
    await page.locator('#auRules .auEd [data-act="addc"]').click();
    await page.selectOption('#auRules .auEd select[data-k="c.1.or"]','1');
    await page.selectOption('#auRules .auEd select[data-k="c.1.s"]','80');
    await page.selectOption('#auRules .auEd select[data-k="a.0.t"]','2');
    await page.selectOption('#auRules .auEd select[data-k="a.0.a"]','1');
    assert.equal(await page.locator('#auRules .auEd .auNoteD').count(),1,'drive-blocked note for mirror fold');
    await page.selectOption('#auRules .auEd select[data-k="a.1.t"]','10');
    await page.selectOption('#auRules .auEd select[data-k="a.2.t"]','7');
    assert.equal(await page.locator('#auRules .auEd select[data-k="a.2.a"] option[value="16"]').isDisabled(),true,'unavailable feature disabled');
    await page.selectOption('#auRules .auEd select[data-k="a.2.a"]','6');
    await page.selectOption('#auRules .auEd select[data-k="a.2.b"]','0');
    await page.fill('#auRules .auEd input[data-k="name"]','Fast');
    await page.locator('#auRules .auEd [data-act="save"]').click();
    await page.waitForFunction(()=>document.querySelector('#auRules .auRow[data-key="r0"]'));
    const post=fw.posts.find(p=>p.startsWith('/api/auto/rule?idx=0'));
    assert.equal(post,'/api/auto/rule?idx=0&name=Fast&r=1,1,0,10,1,2,2,100,-32768,10,1,0,0,1,80,0,1,1,0,0,0,0,0,0,0,0,2,1,0,10,0,5,7,6,0','saved csv');
    // Overflow at phone width with the editor open.
    const over=await page.evaluate(()=>[...document.querySelectorAll('#panelAutomation *')].filter(e=>{const r=e.getBoundingClientRect();return r.width>0&&r.right>window.innerWidth+1}).length);
    assert.equal(over,0,`${label} ${width}px: nothing wider than the screen`);
    // Run now and delete.
    await page.locator('#auRules .auEd [data-act="run"]').click();
    await page.waitForFunction(()=>true);
    await page.locator('#auRules .auEd [data-act="del"]').click();
    await page.waitForFunction(()=>!document.querySelector('#auRules .auRow[data-key="r0"]'));
    assert.ok(fw.posts.includes('/api/auto/run?idx=0')&&fw.posts.includes('/api/auto/delete?idx=0'));
    // Server errors are shown.
    await page.selectOption('#auTplSel','0');
    await page.fill('#auRules .auEd input[data-k="cool"]','0');
    await page.dispatchEvent('#auRules .auEd input[data-k="cool"]','change');
    await page.locator('#auRules .auEd [data-act="save"]').click();
    await page.waitForFunction(()=>/cooldown/.test(document.getElementById('auNote').textContent));
    await page.close();
    }
  }finally{await browser.close()}
}

(async()=>{
  const js=automationJs();
  for(const {label,html}of dashboardArtifacts(root))await check(html,label,js);
  await check(fs.readFileSync(path.join(root,'zh-CN','dashboard_source_zh-CN.html'),'utf8'),'zh-CN',js,true);
  console.log('PASS Automation panel (source, embedded, zh-CN; 390 / 320 px)');
})().catch(e=>{console.error(e);process.exit(1)});
