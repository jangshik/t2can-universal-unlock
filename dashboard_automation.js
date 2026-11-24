// Automation panel (served gzipped as /automation.js, loaded when the panel opens).
// Builds the rule editor from /api/auto/catalog and polls /api/auto.
(()=>{
if(window.AutoUi)return;
const TEXT={
en:{
master:'Automation',masterD:'Run the rules below. Turn off to stop every rule at once.',
listenOnly:'Listen-only is on: rules that send CAN frames are skipped.',
unsupported:'Automation needs Body + Chassis (Juniper) or Model Y L.',
stats:(n,run,blk)=>`${n} rule(s) · ${run} action(s) run · ${blk} blocked in D`,
flags:'FLAGS',flagsD:'Switches you can use in rules (e.g. a car-wash mode). Rules can set them too.',
rules:'RULES',add:'Add rule',tpl:'Add from template…',empty:'No rules yet.',
log:'RECENT ACTIVITY',logEmpty:'Nothing has run yet.',
name:'NAME',enabled:'Enabled',dry:'Test mode (dry-run)',dryD:'Log what would run without sending anything.',
restart:'Restart when triggered again',restartD:'Otherwise a new trigger is ignored while the actions run.',
when:'WHEN',if:'IF',then:'THEN',cool:'COOLDOWN (s)',hold:'FOR (s)',
tState:'becomes',tChange:'changes',from:'from',to:'to',any:'any',
addCond:'+ Condition',noCond:'Always (no conditions)',and:'AND',or:'OR',del:'Remove',
save:'Save',run:'Run now',remove:'Delete rule',cancel:'Close',saved:'Saved',failed:'Failed',
confirmDel:'Delete this rule?',newRule:'New rule',notInD:'Not run in D',
precedence:'AND is evaluated before OR: A AND B OR C = (A AND B) OR C.',
newNote:'New rules start in test mode: check the log, then turn test mode off.',
status:(f,ago,last)=>`${f} run(s)${ago>=0?` · ${fmtAgo(ago)} ago`:''}${last?` · ${last}`:''}`,
badgeOff:'OFF',badgeDry:'TEST',badgeRun:'RUNNING',
triggered:'triggered',manual:'run now',ago:'ago',
units:{s:'s',m:'min',h:'h'},
groups:['Driving','Doors & locks','Windows','Weather','Lights & mirrors','2nd-row seat belts','Features','Flags'],
sig:{gear:'Gear',speed:'Speed (km/h)',pedal:'Accelerator (%)',brake:'Brake',doorFL:'Driver door',doorFR:'Passenger door',doorRL:'Rear left door',doorRR:'Rear right door',anyDoor:'Any door',handleFL:'Driver door handle',handleFR:'Passenger door handle',locked:'Car',winFL:'Driver window',winFR:'Passenger window',winRL:'Rear left window',winRR:'Rear right window',anyWindowOpen:'Any window open',rain:'Rain',snow:'Snow',rainOrSnow:'Rain or snow',mirrors:'Mirrors',camera:'Camera view',hazards:'Hazard lights',lightSwitch:'Light switch',belt2L:'Left seat belt',belt2C:'Centre seat belt',belt2R:'Right seat belt'},
feat:['Easy Entry','Easy Entry mirror fold','Seat-belt chime off','Reverse hazards','Reverse rear light','Door-open hazards','Kickdown','Door-handle macro','Steering-wheel macros','Hazard blink','Nag Killer','ISA suppression','Driver monitoring','Auto Blinker','TLSSC','Door button cancel','Visual Speed Control','Continuous AP','Battery preheating','Listen-only'],
featPrefix:'Feature: ',flag:'Flag',
vals:{gear:{1:'P',2:'R',3:'N',4:'D'},brake:['released','pressed'],door:['closed','open'],handle:['released','pulled'],locked:['unlocked','locked'],win:['closed','vent','partly open','fully open'],yesno:['no','yes'],mirrors:['unfolded','folded','moving'],onoff:['off','on'],light:['Auto','On','Parking','Off'],belt:['buckled / empty','unbuckled']},
ops:['=','≠','>','≥','<','≤'],
act:['—','Hazard blink','Mirrors','Windows','Camera view','Open frunk','Rear fog','Feature','Pedal mode','Flag','Wait'],
seconds:'seconds',fold:['Unfold','Fold'],winReq:{2:'Vent',3:'Close',4:'Open'},winAll:'all',doors:['FL','FR','RL','RR'],
fog:['Off','On for'],set:['Off','On','Toggle'],pedal:['Chill','Standard','Sport','Vehicle setting'],
fogNote:'Rear fog flickered on Juniper in reverse tests; check it on your car.',
tpls:[
['Rain: close windows when locked',[1,1,0,60,1,20,0,1,-32768,0,1,0,0,1,12,0,0,1,0,0,0,0,0,0,0,0,3,15,3,0,0,0,0,0,0]],
['Locked with a window open: close it',[1,1,0,30,2,12,0,1,0,0,17,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,3,15,3,0,0,0,0,0,0]],
['Over 100 km/h for 10 s: kickdown off',[1,1,0,60,1,2,2,100,-32768,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,6,0,0,0,0,0,0,0]],
['Car wash mode on (Flag 1)',[1,1,0,5,1,80,0,1,-32768,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,7,1,0,7,5,0]],
['Car wash mode off (Flag 1)',[1,1,0,5,1,80,0,0,-32768,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,1,7,1,1,7,5,1]]]
},
'zh-CN':{
master:'自动化',masterD:'运行下方规则。关闭后所有规则立即停止。',
listenOnly:'仅监听模式已开启：发送 CAN 帧的规则会被跳过。',
unsupported:'自动化需要 Body + Chassis（Juniper）或 Model Y L。',
stats:(n,run,blk)=>`${n} 条规则 · 已执行 ${run} 个动作 · D 挡拦截 ${blk} 次`,
flags:'标志',flagsD:'可在规则中使用的开关（例如洗车模式）。规则也可以设置它们。',
rules:'规则',add:'添加规则',tpl:'从模板添加…',empty:'暂无规则。',
log:'最近活动',logEmpty:'尚未执行任何动作。',
name:'名称',enabled:'启用',dry:'测试模式（不发送）',dryD:'只记录将要执行的动作，不发送任何内容。',
restart:'再次触发时重新开始',restartD:'否则动作执行期间的新触发会被忽略。',
when:'当',if:'如果',then:'则',cool:'冷却 (s)',hold:'持续 (s)',
tState:'变为',tChange:'变化',from:'从',to:'到',any:'任意',
addCond:'+ 条件',noCond:'始终（无条件）',and:'并且',or:'或者',del:'移除',
save:'保存',run:'立即运行',remove:'删除规则',cancel:'关闭',saved:'已保存',failed:'失败',
confirmDel:'删除此规则？',newRule:'新规则',notInD:'D 挡不执行',
precedence:'先计算“并且”再计算“或者”：A 并且 B 或者 C = (A 并且 B) 或者 C。',
newNote:'新规则以测试模式开始：查看记录后再关闭测试模式。',
status:(f,ago,last)=>`执行 ${f} 次${ago>=0?` · ${fmtAgo(ago)}前`:''}${last?` · ${last}`:''}`,
badgeOff:'关',badgeDry:'测试',badgeRun:'运行中',
triggered:'已触发',manual:'手动运行',ago:'前',
units:{s:'秒',m:'分',h:'小时'},
groups:['行驶','车门与锁','车窗','天气','灯光与后视镜','第二排安全带','功能','标志'],
sig:{gear:'挡位',speed:'车速 (km/h)',pedal:'加速踏板 (%)',brake:'制动',doorFL:'驾驶员车门',doorFR:'副驾车门',doorRL:'左后车门',doorRR:'右后车门',anyDoor:'任一车门',handleFL:'驾驶员门把手',handleFR:'副驾门把手',locked:'车辆',winFL:'驾驶员车窗',winFR:'副驾车窗',winRL:'左后车窗',winRR:'右后车窗',anyWindowOpen:'有车窗打开',rain:'雨',snow:'雪',rainOrSnow:'雨或雪',mirrors:'后视镜',camera:'摄像头画面',hazards:'双闪',lightSwitch:'灯光开关',belt2L:'左侧安全带',belt2C:'中间安全带',belt2R:'右侧安全带'},
feat:['便捷上车','便捷上车折叠后视镜','关闭安全带提示','倒车双闪','倒车后灯','开门双闪','Kickdown','门把手宏','方向盘宏','双闪闪烁','Nag Killer','ISA 抑制','驾驶员监控','Auto Blinker','TLSSC','车门按钮取消','视觉限速控制','Continuous AP','电池预热','仅监听'],
featPrefix:'功能：',flag:'标志',
vals:{gear:{1:'P',2:'R',3:'N',4:'D'},brake:['松开','踩下'],door:['关闭','打开'],handle:['松开','拉动'],locked:['未锁','已锁'],win:['关闭','通风','部分打开','完全打开'],yesno:['否','是'],mirrors:['展开','折叠','移动中'],onoff:['关','开'],light:['自动','开','驻车','关'],belt:['已系 / 无人','未系']},
ops:['=','≠','>','≥','<','≤'],
act:['—','双闪闪烁','后视镜','车窗','摄像头画面','打开前备箱','后雾灯','功能','踏板模式','标志','等待'],
seconds:'秒',fold:['展开','折叠'],winReq:{2:'通风',3:'关闭',4:'打开'},winAll:'全部',doors:['左前','右前','左后','右后'],
fog:['关','开启'],set:['关','开','切换'],pedal:['舒适','标准','运动','车辆设置'],
fogNote:'Juniper 倒车测试中后雾灯会闪烁，请在您的车上确认。',
tpls:[
['下雨：锁车时关闭车窗',[1,1,0,60,1,20,0,1,-32768,0,1,0,0,1,12,0,0,1,0,0,0,0,0,0,0,0,3,15,3,0,0,0,0,0,0]],
['锁车时有车窗打开：关闭',[1,1,0,30,2,12,0,1,0,0,17,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,3,15,3,0,0,0,0,0,0]],
['车速超过 100 km/h 持续 10 秒：关闭 Kickdown',[1,1,0,60,1,2,2,100,-32768,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,6,0,0,0,0,0,0,0]],
['洗车模式开（标志 1）',[1,1,0,5,1,80,0,1,-32768,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,7,1,0,7,5,0]],
['洗车模式关（标志 1）',[1,1,0,5,1,80,0,0,-32768,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,1,7,1,1,7,5,1]]]
}};
const T=TEXT[window.AUTO_LANG]||TEXT.en;
const ANY=-32768,KB=0,KE=1,KN=2;
const esc=s=>String(s??'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function fmtAgo(s){return s<60?s+T.units.s:s<3600?Math.floor(s/60)+T.units.m:Math.floor(s/3600)+T.units.h}
let root=null,cat=null,st=null,sigs={},openKey=null,draft=null,draftIdx=-1,listSig='',note='',noteBad=false,inflight=0;

// ─── catalog helpers ─────────────────────────────────────────────────────
function sigName(id){const s=sigs[id];if(!s)return'?';
  if(s.key.startsWith('feature.')){const f=id-40;return T.featPrefix+(T.feat[f]||s.key)}
  if(s.key.startsWith('flag'))return T.flag+' '+s.key.slice(4);
  return T.sig[s.key]||s.key}
function valSet(id){const s=sigs[id];if(!s)return null;const k=s.key;
  if(k==='gear')return T.vals.gear;if(k==='brake')return T.vals.brake;
  if(k.startsWith('door')||k==='anyDoor')return T.vals.door;if(k.startsWith('handle'))return T.vals.handle;
  if(k==='locked')return T.vals.locked;if(/^win(FL|FR|RL|RR)$/.test(k))return T.vals.win;
  if(k==='mirrors')return T.vals.mirrors;if(k==='lightSwitch')return T.vals.light;if(k.startsWith('belt'))return T.vals.belt;
  if(k==='anyWindowOpen'||k==='rain'||k==='snow'||k==='rainOrSnow')return T.vals.yesno;
  return T.vals.onoff}
function valName(id,v){if(v===ANY)return T.any;const s=sigs[id];if(!s)return String(v);if(s.kind===KN)return String(v);const set=valSet(id);return(set&&set[v]!==undefined)?set[v]:String(v)}
function groupOf(k){if(['gear','speed','pedal','brake'].includes(k))return 0;if(k.startsWith('door')||k==='anyDoor'||k.startsWith('handle')||k==='locked')return 1;
  if(k.startsWith('win')||k==='anyWindowOpen')return 2;if(['rain','snow','rainOrSnow'].includes(k))return 3;if(k.startsWith('belt'))return 5;
  if(k.startsWith('feature.'))return 6;if(k.startsWith('flag'))return 7;return 4}
function sigOptions(sel,allowNone){const groups=T.groups.map(()=>[]);
  for(const s of cat.signals)groups[groupOf(s.key)].push(s);
  let h=allowNone?`<option value="0"${sel===0?' selected':''}>—</option>`:'';
  groups.forEach((g,i)=>{if(!g.length)return;h+=`<optgroup label="${esc(T.groups[i])}">`+g.map(s=>{
    const unavail=s.key.startsWith('feature.')&&cat.featureAvailable&&!cat.featureAvailable[s.id-40];
    return`<option value="${s.id}"${s.id===sel?' selected':''}${unavail?' disabled':''}>${esc(sigName(s.id))}</option>`}).join('')+'</optgroup>'});
  return h}
function optList(pairs,sel){return pairs.map(([v,l,dis])=>`<option value="${v}"${v===sel?' selected':''}${dis?' disabled':''}>${esc(l)}</option>`).join('')}
function valuePairs(id,withAny){const s=sigs[id];if(!s)return[];const out=withAny?[[ANY,T.any]]:[];
  for(let v=s.min;v<=s.max;v++)out.push([v,valName(id,v)]);return out}
function driveBlocked(a){return(cat.driveBlocked||[]).some(([t,p])=>t===a.t&&(p===255||p===a.a))}

// ─── rule <-> csv ────────────────────────────────────────────────────────
function fromCsv(name,r){let i=0;const d={name,en:!!r[i++],dry:!!r[i++],restart:!!r[i++],cool:r[i++],tt:r[i++],ts:r[i++],to:r[i++],tv:r[i++],tf:r[i++],th:r[i++],cond:[],act:[]};
  for(let c=0;c<4;c++){const s=r[i++],op=r[i++],or=r[i++],v=r[i++];if(s)d.cond.push({s,op,or,v})}
  for(let a=0;a<3;a++){d.act.push({t:r[i++],a:r[i++],b:r[i++]})}return d}
function toCsv(d){const r=[d.en?1:0,d.dry?1:0,d.restart?1:0,d.cool,d.tt,d.ts,d.tt===1?d.to:0,d.tv,d.tt===2?d.tf:ANY,d.th];
  for(let c=0;c<4;c++){const x=d.cond[c];if(x)r.push(x.s,x.op,c?x.or:0,x.v);else r.push(0,0,0,0)}
  for(let a=0;a<3;a++){const x=d.act[a]||{t:0,a:0,b:0};r.push(x.t,x.t?x.a:0,x.t?x.b:0)}return r.join(',')}

// ─── text ────────────────────────────────────────────────────────────────
function actText(a){if(!a||!a.t)return'';const n=T.act[a.t]||'?';
  switch(a.t){case 1:return`${n} ${a.b}${T.units.s}`;case 2:return T.fold[a.a]+' '+n.toLowerCase();
  case 3:return`${n} (${a.a===15?T.winAll:T.doors.filter((_,i)=>(a.a>>i)&1).join(' ')}): ${T.winReq[a.b]||'?'}`;
  case 6:return a.a?`${n} ${a.b}${T.units.s}`:`${n}: ${T.fog[0]}`;case 7:return`${T.feat[a.a]||'?'}: ${T.set[a.b]}`;
  case 8:return`${n}: ${T.pedal[a.a]}`;case 9:return`${T.flag} ${a.a+1}: ${T.set[a.b]}`;case 10:return`${n} ${a.b}${T.units.s}`;default:return n}}
function condText(c){return`${sigName(c.s)} ${T.ops[c.op]} ${valName(c.s,c.v)}`}
function summary(d){let w=d.tt===2?`${sigName(d.ts)}: ${valName(d.ts,d.tf)} → ${valName(d.ts,d.tv)}`:`${sigName(d.ts)} ${T.ops[d.to]} ${valName(d.ts,d.tv)}`;
  if(d.th)w+=` (${d.th}${T.units.s})`;
  const c=d.cond.map((x,i)=>(i?` ${x.or?T.or:T.and} `:'')+condText(x)).join('');
  const a=d.act.filter(x=>x.t).map(actText).join(' · ');
  return`${T.when} ${w}${c?` · ${T.if} ${c}`:''} → ${a}`}

// ─── network ─────────────────────────────────────────────────────────────
async function post(url){inflight++;try{const r=await fetch(url,{method:'POST',cache:'no-store'});let j=null;try{j=await r.json()}catch(e){}
  if(!r.ok||!j||j.ok===false)throw new Error((j&&j.error)||('HTTP '+r.status));return j}finally{inflight--}}
async function refresh(){if(!root)return;try{const r=await fetch('/api/auto',{cache:'no-store'});const j=await r.json();if(inflight)return;st=j;render()}catch(e){}}
function setNote(t,bad){note=t||'';noteBad=!!bad;const n=root&&root.querySelector('#auNote');if(n){n.textContent=note;n.classList.toggle('auBad',noteBad)}}

// ─── rendering ───────────────────────────────────────────────────────────
const CSS=`.auChips{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;padding:12px}.auChip{border:1px solid var(--line);background:transparent;color:var(--field-ink,var(--ink));border-radius:14px;padding:9px 4px;font:inherit;font-size:10.5px;font-weight:650}.auChip.on{background:var(--green);border-color:var(--green);color:#fff}
.auRow+.auRow{border-top:1px solid var(--line)}.auHead{display:flex;gap:10px;align-items:flex-start;width:100%;padding:12px 15px;background:none;border:0;color:inherit;font:inherit;text-align:left;cursor:pointer}.auHead:focus-visible{outline:2px solid var(--field-ink,var(--ink));outline-offset:-2px}
.auMain{flex:1;min-width:0}.auName{font-size:12px;font-weight:660;display:flex;gap:6px;align-items:center;flex-wrap:wrap}.auSum{font-size:9.5px;color:var(--muted);line-height:1.45;margin-top:3px;overflow-wrap:anywhere}.auSt{font-size:9px;color:var(--muted);margin-top:3px}
.auBadge{font-size:8px;font-weight:760;border-radius:99px;padding:2px 6px;background:rgba(127,127,127,.16);color:var(--muted)}.auBadge.dry{background:rgba(var(--accent-rgb,80,120,255),.16);color:var(--accent,var(--blue))}.auBadge.run{background:var(--green);color:#fff}
.auChev{font-size:14px;color:var(--muted);transition:transform .15s;margin-top:1px}.auRow.open .auChev{transform:rotate(90deg)}
.auEd{padding:2px 13px 13px}.auSec{font-size:9px;color:var(--muted);font-weight:720;letter-spacing:.08em;margin:12px 2px 6px}.auLine{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin:0 0 6px}
.auLine select,.auLine input[type=number],.auLine input[type=text]{border:1px solid var(--line);background:transparent;color:var(--field-ink,var(--ink));border-radius:12px;padding:7px 8px;font:inherit;font-size:11px;min-width:0;max-width:100%}
.auLine select{flex:1 1 130px}.auLine select.auSm{flex:0 1 auto}.auLine input[type=number]{width:72px}.auLine input[type=text]{flex:1 1 100%}.auLine label{font-size:10.5px;display:flex;gap:4px;align-items:center}
.auTxt{font-size:10px;color:var(--muted)}.auX{border:0;background:transparent;color:var(--red);font:inherit;font-size:10px;padding:6px}.auNoteD{font-size:9px;color:var(--amber);margin:-2px 2px 6px}
.auBtns{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}.auBtns .btn{width:100%}.auBad{color:var(--red)!important}.auLog{padding:4px 15px 10px}.auLogRow{font-size:9.5px;line-height:1.5;padding:5px 0;border-top:1px solid var(--line);color:var(--field-ink,var(--ink))}.auLogRow:first-child{border-top:0}.auLogRow span{color:var(--muted)}
.auTpl{display:flex;gap:8px;padding:12px}.auTpl .btn{flex:0 0 auto}.auTpl select{flex:1;min-width:0;border:1px solid var(--line);background:transparent;color:var(--field-ink,var(--ink));border-radius:13px;padding:8px;font:inherit;font-size:11px}`;
function tog(id,on,key,desc,dis){return`<div class="settingToggleRow"><div><div class="listkey">${esc(key)}</div>${desc?`<div class="listdesc">${esc(desc)}</div>`:''}</div><label class="toggle"><input id="${id}" type="checkbox"${on?' checked':''}${dis?' disabled':''}/><span class="track"></span></label></div>`}
function skeleton(){root.innerHTML=`<style>${CSS}</style>
<section class="card" id="auMasterCard">${tog('auMaster',false,T.master,T.masterD)}<div class="softNote" id="auStatus" style="padding:0 15px 12px"></div></section>
<div class="detailSectionLabel">${esc(T.flags)}</div><section class="card"><div class="auChips" id="auFlags"></div></section><div class="softNote">${esc(T.flagsD)}</div>
<div class="detailSectionLabel">${esc(T.rules)}</div><section class="card"><div id="auRules"></div>
<div class="auTpl"><button class="btn primary" id="auAdd">${esc(T.add)}</button><select id="auTplSel" aria-label="${esc(T.tpl)}"><option value="">${esc(T.tpl)}</option>${T.tpls.map((t,i)=>`<option value="${i}">${esc(t[0])}</option>`).join('')}</select></div></section>
<div class="softNote" id="auNote" aria-live="polite"></div>
<div class="detailSectionLabel">${esc(T.log)}</div><section class="card"><div class="auLog" id="auLog"></div></section>`;
  root.querySelector('#auMaster').onchange=async e=>{const el=e.target;try{st=await post('/api/auto/global?enabled='+(el.checked?1:0));render()}catch(x){el.checked=!el.checked;setNote(T.failed+' · '+x.message,true)}};
  root.querySelector('#auAdd').onclick=()=>startNew(null);
  root.querySelector('#auTplSel').onchange=e=>{const i=e.target.value;e.target.value='';if(i!=='')startNew(T.tpls[+i])};
  root.querySelector('#auFlags').onclick=async e=>{const b=e.target.closest('[data-flag]');if(!b)return;const i=+b.dataset.flag;const on=!b.classList.contains('on');
    try{st=await post(`/api/auto/flag?idx=${i}&value=${on?1:0}`);render()}catch(x){setNote(T.failed+' · '+x.message,true)}};
  root.querySelector('#auRules').addEventListener('click',onRulesClick);
  root.querySelector('#auRules').addEventListener('change',onEditorChange);
  root.querySelector('#auRules').addEventListener('input',e=>{if(e.target.dataset.k==='name'&&draft)draft.name=e.target.value})}
function freeIdx(){const used=new Set((st.rules||[]).map(r=>r.idx));for(let i=0;i<(st.ruleMax||16);i++)if(!used.has(i))return i;return-1}
function startNew(tpl){if(!st)return;const idx=freeIdx();if(idx<0){setNote(T.failed+' · 16/16',true);return}
  draft=tpl?fromCsv(tpl[0],tpl[1]):{name:T.newRule,en:true,dry:true,restart:false,cool:10,tt:1,ts:9,to:0,tv:1,tf:ANY,th:0,cond:[],act:[{t:1,a:0,b:3},{t:0,a:0,b:0},{t:0,a:0,b:0}]};
  draft.dry=true;draftIdx=idx;openKey='n'+idx;listSig='';setNote(T.newNote);render()}
function render(){if(st)window.autoRowUpdate?.(st);if(!root||!st||!cat)return;const m=root.querySelector('#auMaster');if(m&&document.activeElement!==m){m.checked=!!st.enabled;m.disabled=!st.supported}
  const rules=st.rules||[];const s=st.stats||{};
  const status=root.querySelector('#auStatus');status.textContent=!st.supported?T.unsupported:(st.listenOnly?T.listenOnly+' ':'')+T.stats(rules.length,s.actionsRun||0,s.driveBlocked||0);
  status.classList.toggle('auBad',!st.supported||!!st.listenOnly);
  root.querySelector('#auFlags').innerHTML=Array.from({length:cat.flagCount||8},(_,i)=>`<button class="auChip${(st.flags>>i)&1?' on':''}" data-flag="${i}" aria-pressed="${(st.flags>>i)&1?'true':'false'}">${esc(T.flag)} ${i+1}</button>`).join('');
  const sig=JSON.stringify([rules.map(r=>[r.idx,r.name,r.r]),openKey]);
  if(sig!==listSig){listSig=sig;renderList()}else updateRowStatus();
  const log=st.log||[];const byIdx={};rules.forEach(r=>byIdx[r.idx]=r.name);
  root.querySelector('#auLog').innerHTML=log.length?log.map(e=>`<div class="auLogRow"><span>${esc(fmtAgo(e.agoS))} ${esc(T.ago)} · ${esc(byIdx[e.rule]??('#'+(e.rule+1)))}${e.dry?' · TEST':''}</span><br>${esc(e.type===255?T.triggered:e.type===254?T.manual:actText({t:e.type,a:e.a,b:e.b}))} — ${esc(e.result)}</div>`).join(''):`<div class="auLogRow"><span>${esc(T.logEmpty)}</span></div>`}
function rowStatus(r){return T.status(r.fires,r.lastAgoS,r.last)}
function badges(d,r){return(!d.en?`<span class="auBadge">${esc(T.badgeOff)}</span>`:'')+(d.dry?`<span class="auBadge dry">${esc(T.badgeDry)}</span>`:'')+(r&&r.running?`<span class="auBadge run">${esc(T.badgeRun)}</span>`:'')}
function renderList(){const box=root.querySelector('#auRules');const rules=st.rules||[];let h='';
  if(openKey&&openKey[0]==='n')h+=rowHtml('n'+draftIdx,draft,null);
  for(const r of rules){const d=fromCsv(r.name,r.r);h+=rowHtml('r'+r.idx,openKey==='r'+r.idx&&draft?draft:d,r)}
  if(!h)h=`<div class="softNote" style="padding:14px 15px">${esc(T.empty)}</div>`;box.innerHTML=h;
  const ed=box.querySelector('.auEd');if(ed)renderEditor(ed)}
function rowHtml(key,d,r){const open=openKey===key;return`<div class="auRow${open?' open':''}" data-key="${key}"><button class="auHead" data-act="toggle" aria-expanded="${open}"><span class="auChev">›</span><span class="auMain"><span class="auName">${esc(d.name||'#')}${badges(d,r)}</span><span class="auSum" style="display:block">${esc(summary(d))}</span>${r?`<span class="auSt" id="auSt${r.idx}" style="display:block">${esc(rowStatus(r))}</span>`:''}</span></button>${open?'<div class="auEd"></div>':''}</div>`}
function updateRowStatus(){for(const r of st.rules||[]){const el=root.querySelector('#auSt'+r.idx);if(el)el.textContent=rowStatus(r)}}
function valueCtl(k,id,v,withAny){const s=sigs[id];if(!s)return'';if(s.kind===KN&&!withAny)return`<input type="number" data-k="${k}" min="${s.min}" max="${s.max}" step="1" value="${v}"/>`;
  if(s.kind===KN)return`<select class="auSm" data-k="${k}">${optList([[ANY,T.any]],v)}</select><input type="number" data-k="${k}N" min="${s.min}" max="${s.max}" step="1" value="${v===ANY?'':v}"/>`;
  return`<select class="auSm" data-k="${k}">${optList(valuePairs(id,withAny),v)}</select>`}
function opCtl(k,id,op){const s=sigs[id];const ops=s&&s.kind===KN?[0,1,2,3,4,5]:[0,1];return`<select class="auSm" data-k="${k}" aria-label="op">${optList(ops.map(o=>[o,T.ops[o]]),op)}</select>`}
function actCtl(i,a){let h=`<div class="auLine"><select data-k="a.${i}.t" aria-label="${esc(T.then)}">${optList(T.act.map((n,t)=>[t,n]),a.t)}</select>`;
  switch(a.t){case 1:case 10:h+=`<input type="number" data-k="a.${i}.b" min="1" max="${a.t===1?60:600}" value="${a.b}"/><span class="auTxt">${esc(T.seconds)}</span>`;break;
  case 2:h+=`<select class="auSm" data-k="a.${i}.a">${optList(T.fold.map((n,v)=>[v,n]),a.a)}</select>`;break;
  case 3:h+=T.doors.map((n,b)=>`<label><input type="checkbox" data-k="a.${i}.m${b}"${(a.a>>b)&1?' checked':''}/>${esc(n)}</label>`).join('')+`<select class="auSm" data-k="a.${i}.b">${optList([2,3,4].map(v=>[v,T.winReq[v]]),a.b)}</select>`;break;
  case 6:h+=`<select class="auSm" data-k="a.${i}.a">${optList(T.fog.map((n,v)=>[v,n]),a.a)}</select>`+(a.a?`<input type="number" data-k="a.${i}.b" min="1" max="3600" value="${a.b}"/><span class="auTxt">${esc(T.seconds)}</span>`:'');break;
  case 7:h+=`<select data-k="a.${i}.a">${optList(T.feat.map((n,v)=>[v,n,cat.featureAvailable&&!cat.featureAvailable[v]]),a.a)}</select><select class="auSm" data-k="a.${i}.b">${optList(T.set.map((n,v)=>[v,n]),a.b)}</select>`;break;
  case 8:h+=`<select class="auSm" data-k="a.${i}.a">${optList(T.pedal.map((n,v)=>[v,n]),a.a)}</select>`;break;
  case 9:h+=`<select class="auSm" data-k="a.${i}.a">${optList(Array.from({length:cat.flagCount||8},(_,v)=>[v,T.flag+' '+(v+1)]),a.a)}</select><select class="auSm" data-k="a.${i}.b">${optList(T.set.map((n,v)=>[v,n]),a.b)}</select>`;break}
  h+='</div>';if(a.t&&driveBlocked(a))h+=`<div class="auNoteD">${esc(T.notInD)}</div>`;if(a.t===6&&a.a)h+=`<div class="auNoteD">${esc(T.fogNote)}</div>`;return h}
function renderEditor(ed){const d=draft;if(!d)return;const isNew=openKey[0]==='n';
  let h=`<div class="auSec">${esc(T.name)}</div><div class="auLine"><input type="text" data-k="name" maxlength="31" value="${esc(d.name)}"/></div>`;
  h+=tog('auEn',d.en,T.enabled,'')+tog('auDry',d.dry,T.dry,T.dryD)+tog('auRs',d.restart,T.restart,T.restartD);
  h+=`<div class="auSec">${esc(T.when)}</div><div class="auLine"><select data-k="ts">${sigOptions(d.ts,false)}</select><select class="auSm" data-k="tt">${optList([[1,T.tState],[2,T.tChange]],d.tt)}</select></div>`;
  if(d.tt===1)h+=`<div class="auLine">${opCtl('to',d.ts,d.to)}${valueCtl('tv',d.ts,d.tv,false)}</div>`;
  else h+=`<div class="auLine"><span class="auTxt">${esc(T.from)}</span>${valueCtl('tf',d.ts,d.tf,true)}<span class="auTxt">${esc(T.to)}</span>${valueCtl('tv',d.ts,d.tv,true)}</div>`;
  h+=`<div class="auLine"><span class="auTxt">${esc(T.hold)}</span><input type="number" data-k="th" min="0" max="250" value="${d.th}"/></div>`;
  h+=`<div class="auSec">${esc(T.if)}</div>`;
  if(!d.cond.length)h+=`<div class="auLine"><span class="auTxt">${esc(T.noCond)}</span></div>`;
  d.cond.forEach((c,i)=>{h+=`<div class="auLine">${i?`<select class="auSm" data-k="c.${i}.or">${optList([[0,T.and],[1,T.or]],c.or)}</select>`:''}<select data-k="c.${i}.s">${sigOptions(c.s,false)}</select>${opCtl(`c.${i}.op`,c.s,c.op)}${valueCtl(`c.${i}.v`,c.s,c.v,false)}<button class="auX" data-act="delc" data-i="${i}">${esc(T.del)}</button></div>`});
  if(d.cond.length<(cat.condMax||4))h+=`<div class="auLine"><button class="btn" data-act="addc">${esc(T.addCond)}</button></div>`;
  if(d.cond.some((c,i)=>i&&c.or)&&d.cond.length>2)h+=`<div class="auNoteD" style="color:var(--muted)">${esc(T.precedence)}</div>`;
  h+=`<div class="auSec">${esc(T.then)}</div>`+d.act.map((a,i)=>actCtl(i,a)).join('');
  h+=`<div class="auSec">${esc(T.cool)}</div><div class="auLine"><input type="number" data-k="cool" min="1" max="3600" value="${d.cool}"/></div>`;
  h+=`<div class="auBtns"><button class="btn primary" data-act="save">${esc(T.save)}</button><button class="btn" data-act="close">${esc(T.cancel)}</button>`+(isNew?'':`<button class="btn" data-act="run">${esc(T.run)}</button><button class="btn" data-act="del" style="color:var(--red)">${esc(T.remove)}</button>`)+'</div>';
  ed.innerHTML=h}
function sigDefault(id){const s=sigs[id];return s?(s.kind===KB?1:s.min):0}
function actDefault(t){return({1:{a:0,b:3},2:{a:1,b:0},3:{a:15,b:3},6:{a:1,b:60},7:{a:0,b:1},8:{a:0,b:0},9:{a:0,b:1},10:{a:0,b:5}})[t]||{a:0,b:0}}
function onEditorChange(e){const el=e.target;if(!draft)return;const id=el.id;
  if(id==='auEn'||id==='auDry'||id==='auRs'){draft[id==='auEn'?'en':id==='auDry'?'dry':'restart']=el.checked;refreshHead();return}
  const k=el.dataset.k;if(!k||k==='name')return;const num=v=>v===''?0:Number(v);let m;
  if(k==='ts'){draft.ts=+el.value;draft.to=0;draft.tv=sigDefault(draft.ts);draft.tf=ANY}
  else if(k==='tt'){draft.tt=+el.value;if(draft.tt===2){draft.tf=ANY;if(sigs[draft.ts]?.kind!==KN||draft.tv===ANY)draft.tv=sigDefault(draft.ts)}}
  else if(k==='tvN'||k==='tfN'){draft[k[1]==='v'?'tv':'tf']=el.value===''?ANY:num(el.value)}
  else if(['to','tv','tf','th','cool'].includes(k))draft[k]=num(el.value);
  else if((m=k.match(/^c\.(\d)\.(s|op|or|v)$/))){const c=draft.cond[+m[1]];c[m[2]]=num(el.value);if(m[2]==='s'){c.op=0;c.v=sigDefault(c.s)}}
  else if((m=k.match(/^a\.(\d)\.m(\d)$/))){const a=draft.act[+m[1]];a.a=el.checked?(a.a|(1<<+m[2])):(a.a&~(1<<+m[2]))}
  else if((m=k.match(/^a\.(\d)\.(t|a|b)$/))){const a=draft.act[+m[1]];a[m[2]]=num(el.value);if(m[2]==='t')Object.assign(a,actDefault(a.t));if(a.t===6&&m[2]==='a'&&a.a&&!a.b)a.b=60}
  rerenderEditor()}
function refreshHead(){const row=root.querySelector(`.auRow[data-key="${openKey}"]`);if(!row||!draft)return;const n=row.querySelector('.auName');const r=(st.rules||[]).find(x=>'r'+x.idx===openKey);
  n.innerHTML=esc(draft.name||'#')+badges(draft,r);row.querySelector('.auSum').textContent=summary(draft)}
function rerenderEditor(){const ed=root.querySelector('.auEd');if(ed){const y=window.scrollY;renderEditor(ed);refreshHead();window.scrollTo(0,y)}}
async function onRulesClick(e){const b=e.target.closest('[data-act]');if(!b)return;const act=b.dataset.act;
  if(act==='toggle'){const key=b.closest('.auRow').dataset.key;if(openKey===key){openKey=null;draft=null}else{if(openKey&&openKey[0]==='n'){draft=null}openKey=key;const r=(st.rules||[]).find(x=>'r'+x.idx===key);draft=r?fromCsv(r.name,r.r):draft;draftIdx=r?r.idx:draftIdx}listSig='';render();return}
  if(!draft)return;
  if(act==='addc'){draft.cond.push({s:1,op:0,or:0,v:1});rerenderEditor();return}
  if(act==='delc'){draft.cond.splice(+b.dataset.i,1);rerenderEditor();return}
  if(act==='close'){openKey=null;draft=null;listSig='';render();return}
  if(act==='save'){const nameBytes=new TextEncoder().encode(draft.name||'').length;if(nameBytes>(cat.nameBytes||31)){setNote(T.failed+' · name',true);return}
    const idx=openKey[0]==='n'?draftIdx:+openKey.slice(1);
    try{st=await post(`/api/auto/rule?idx=${idx}&name=${encodeURIComponent(draft.name||'')}&r=${toCsv(draft)}`);openKey='r'+idx;const r=st.rules.find(x=>x.idx===idx);draft=r?fromCsv(r.name,r.r):draft;listSig='';setNote(T.saved);render()}
    catch(x){setNote(T.failed+' · '+x.message,true)}return}
  if(act==='run'){try{st=await post('/api/auto/run?idx='+openKey.slice(1));render()}catch(x){setNote(T.failed+' · '+x.message,true)}return}
  if(act==='del'){if(!confirm(T.confirmDel))return;try{st=await post('/api/auto/delete?idx='+openKey.slice(1));openKey=null;draft=null;listSig='';render()}catch(x){setNote(T.failed+' · '+x.message,true)}}}
async function mount(el){root=el;try{if(!cat){const r=await fetch('/api/auto/catalog',{cache:'no-store'});cat=await r.json();sigs={};for(const s of cat.signals)sigs[s.id]=s}
  skeleton();await refresh()}catch(e){root.innerHTML=`<div class="softNote auBad">${esc(T.failed)} · ${esc(e.message||e)}</div>`;root=null}}
window.AutoUi={mount,refresh,summary:d=>summary(d),fromCsv,toCsv,T};
})();
