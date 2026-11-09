const assert=require('node:assert/strict');
const path=require('node:path');
const {launchBrowser,dashboardArtifacts}=require('./browser_test_runtime');
const root=path.resolve(__dirname,'..');
async function check(html,label){
 const browser=await launchBrowser();
 try{
  const page=await browser.newPage({viewport:{width:390,height:844}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  const wifi={ok:true,ssid:'T2CAN-TEST',ip:'192.168.4.1',passwordIsDefault:false,visibilityMode:0,hideAfterSec:60,ssidHidden:false,visibilityKnown:true,visibilityPending:false,visibilityNvsError:false,visibilityFallback:false,visibilityError:'',hideInSec:0,uptimeSec:10};
  let posts=0,fail=false,disconnect=false,credentials=0;
  await page.route('http://wifi.test/**',async route=>{
   const req=route.request(),u=new URL(req.url());let result={};
   if(u.pathname==='/')return route.fulfill({contentType:'text/html',body:html});
   if(u.pathname==='/api/profile/status')result={setupMode:false,profile:2,topology:2,turn:1,nagSupported:true,nagTorqueSupported:true,nagTsl9Supported:true,advancedEapSupported:true,euUnlockSupported:true};
   else if(u.pathname==='/api/features/status')result={lab:true,s3xy:true,doorCancel:false};
   else if(u.pathname==='/api/wifi/status'){if(disconnect)return route.abort();result=wifi;}
   else if(u.pathname==='/api/wifi/visibility'){
    assert.equal(req.method(),'POST');posts++;
    assert(!u.searchParams.has('ssid')&&!u.searchParams.has('password'));
    if(disconnect)return route.abort();
    if(fail){fail=false;return route.fulfill({status:503,contentType:'application/json',body:'{"ok":false,"error":"Visibility settings were not saved"}'});}
    wifi.visibilityMode=Number(u.searchParams.get('mode'));wifi.hideAfterSec=Number(u.searchParams.get('hideAfterSec'));wifi.hideInSec=wifi.visibilityMode===2?Math.max(0,wifi.hideAfterSec-wifi.uptimeSec):0;
    wifi.ssidHidden=wifi.visibilityMode===1||(wifi.visibilityMode===2&&!wifi.hideInSec);result=wifi;
   }else if(u.pathname==='/api/wifi/apply'){credentials++;result={ok:true,reconnecting:false};}
   await route.fulfill({contentType:'application/json',body:JSON.stringify(result)});
  });
  await page.goto('http://wifi.test/',{waitUntil:'domcontentloaded'});
  await page.waitForFunction(()=>universalBootReady);
  await page.getByRole('button',{name:'Settings',exact:true}).click();
  await page.locator('.listrow[data-panel="panelWireless"]').click();
  await page.waitForFunction(()=>wifiVisibilitySaved&&!document.querySelector('#wifiVisibilityApply').disabled);
  assert.equal(await page.locator('#wifiVisibilityMode').inputValue(),'0');assert(await page.locator('#wifiHideDelayRow').isHidden());
  await page.locator('#wifiVisibilityMode').selectOption('2');assert(await page.locator('#wifiHideDelayRow').isVisible());
  await page.locator('#wifiHideAfterSec').fill('120');await page.evaluate(()=>fetchWifi());
  assert.equal(await page.locator('#wifiHideAfterSec').inputValue(),'120'); // polling preserves edits
  await page.locator('#wifiVisibilityApply').click();await page.waitForFunction(()=>!wifiVisibilitySaving&&wifiVisibilitySaved.hideAfterSec===120);
  assert.equal(posts,1);assert((await page.locator('#wifiVisibilityStatus').textContent()).includes('110 s'));
  assert(!await page.locator('#profileRebootScreen').evaluate(el=>el.classList.contains('show')));
  await page.locator('#wifiSsid').fill('unsaved-name'); // saving visibility must not clobber credentials
  await page.locator('#wifiVisibilityMode').selectOption('1');await page.locator('#wifiVisibilityApply').click();
  await page.waitForFunction(()=>!wifiVisibilitySaving&&wifiVisibilitySaved.ssidHidden);
  await page.evaluate(()=>fetchWifi());assert.equal(await page.locator('#wifiSsid').inputValue(),'unsaved-name');assert.equal(credentials,0);
  assert((await page.locator('#wirelessWifiState').textContent()).includes('HIDDEN'));
  await page.locator('#wifiVisibilityMode').selectOption('2');await page.locator('#wifiHideAfterSec').fill('3601');await page.locator('#wifiVisibilityApply').click();assert.equal(posts,2);
  await page.locator('#wifiHideAfterSec').fill('0');await page.locator('#wifiVisibilityApply').click();
  await page.waitForFunction(()=>!wifiVisibilitySaving&&wifiVisibilitySaved.hideAfterSec===0);assert(wifi.ssidHidden);
  fail=true;await page.locator('#wifiVisibilityMode').selectOption('0');await page.locator('#wifiVisibilityApply').click();
  await page.waitForFunction(()=>!wifiVisibilitySaving&&document.querySelector('#wifiVisibilityStatus').textContent.includes('not saved'));
  assert.equal(wifi.visibilityMode,2);assert.equal(await page.locator('#wifiVisibilityMode').inputValue(),'0');
  await page.locator('#wifiVisibilityApply').click();await page.waitForFunction(()=>!wifiVisibilitySaving&&wifiVisibilitySaved.visibilityMode===0);assert(!wifi.ssidHidden);
  disconnect=true;await page.locator('#wifiVisibilityMode').selectOption('1');await page.locator('#wifiVisibilityApply').click();
  await page.waitForFunction(()=>wifiVisibilityUncertain);assert(await page.locator('#wifiVisibilityApply').isDisabled());
  disconnect=false;await page.evaluate(()=>fetchWifi());assert(!await page.locator('#wifiVisibilityApply').isDisabled());assert.equal(await page.locator('#wifiVisibilityMode').inputValue(),'1');
  wifi.visibilityError='ESP_FAIL';await page.evaluate(()=>fetchWifi());assert((await page.locator('#wifiVisibilityStatus').textContent()).includes('Save to retry'));
  assert.equal(credentials,0);assert(!await page.locator('#profileRebootScreen').evaluate(el=>el.classList.contains('show')));
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
  assert.deepEqual(errors,[]);
  console.log('PASS Wi-Fi visibility dashboard '+label);
 }finally{await browser.close();}
}
(async()=>{for(const {html,label} of dashboardArtifacts(root))await check(html,label)})().catch(e=>{console.error(e);process.exit(1)});
