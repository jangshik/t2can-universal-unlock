#!/usr/bin/env python3
"""Insert the Body Comfort panel and Wi-Fi timing card into the dashboards.

Idempotent: a source that already contains the panel is left untouched.
Usage: python3 tools/add_body_comfort_dashboard.py
Then rebuild the embedded header with tools/build_dashboard.py.
"""
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
MARKER = 'id="panelBodyComfort"'

TEXT = {
    "en": {
        "back": "Back", "settings": "Settings",
        "rowKey": "Body Comfort",
        "rowDesc": "Easy Entry, hazards, rear belts and wheel macros.",
        "title": "Body Comfort",
        "sub": "HW4 Model Y Juniper body features. Each feature stays OFF until enabled. Commands are copies of live vehicle frames on the Body CAN.",
        "unsupported": "Requires Body + Chassis (Model Y Juniper) or Model Y L. This vehicle profile cannot use these features.",
        "experimental": "This profile is not Juniper / Model Y L: the frame layout is unverified, treat every feature as experimental.",
        "ee": "Easy Entry",
        "eeD": "In P, an opening door lowers its window fully. Closing the door returns it to the previous position.",
        "doors": ["Front left door", "Front right door", "Rear left door", "Rear right door"],
        "wx": "Block in rain / snow", "wxD": "Uses the vehicle rain / snow detection.",
        "wxs": "Block when weather is unknown", "wxsD": "Fail closed if the weather signal is missing.",
        "rd": "RESTORE DELAY (ms)",
        "mf": "Fold mirrors", "mfD": "Fold the side mirrors when Easy Entry acts.",
        "cam": "Camera while folded", "camD": "Show the camera view in D/N while this feature has the mirrors folded.",
        "uk": "UNFOLD ABOVE (km/h)", "uh": "FOR (s)",
        "ufD": "Mirrors folded by this feature unfold in D after the speed stays above this value for this time. Mirrors you fold yourself are never unfolded.",
        "sb": "Rear Seat-Belt Chime Off", "sbD": "Suppress the unbuckled chime for the selected 2nd-row seats.",
        "seats": ["2nd row left", "2nd row centre", "2nd row right"],
        "mc": "Steering Wheel Macros",
        "mcD": "Double, triple or long press on a wheel button runs an action, including toggling existing features such as Nag Killer or TLSSC: 8 buttons × 3 gestures = 24 slots. The button's own stock function still runs on each press.",
        "lp": "LONG PRESS (ms)", "gp": "CLICK GAP (ms)",
        "btn": ["Left scroll · press", "Right scroll · press", "Left scroll · tilt left",
                "Left scroll · tilt right", "Right scroll · tilt left", "Right scroll · tilt right",
                "Voice button", "Camera button", "Driver door handle", "Passenger door handle"],
        "ges": ["Double", "Triple", "Long"],
        "act": ["None", "Toggle Easy Entry", "Toggle mirror fold", "Toggle rear belt chime",
                "", "Toggle reverse hazards", "Toggle reverse light",
                "Toggle door hazards", "Hazard blink", "Fold / unfold mirrors", "Camera view",
                "Toggle Nag Killer", "Toggle ISA Suppression", "Toggle Disable Driver Monitoring",
                "Toggle Auto Blinker", "Toggle TLSSC", "Toggle Door Button Cancel",
                "Toggle Visual Speed Control", "Toggle Continuous AP", "Toggle Battery Preheating",
                "Open frunk", "Toggle Listen-Only", "Toggle Kickdown"],
        "actNew": "Body Comfort", "actOld": "Existing features",
        "notSet": "Not set", "setCount": "{n} set", "collapsedHint": "Tap a button to edit its actions.",
        "hb": "Hazard Blink", "hbT": "Hazard Blink (for {n} seconds)",
        "hbD": "One-shot command: a macro or Blink Now turns the hazards on for the set time, then off. This switch only allows or blocks the command; it never leaves the hazards on. Nothing happens if the hazards are already on.",
        "hc": "DURATION (s)", "run": "Blink Now",
        "stMacro": "Last macro",
        "kd": "Kickdown",
        "kdD": "In D, pressing the accelerator to the trigger level switches to a stronger acceleration mode (same mechanism as Pedal Response). It switches back when a release condition below is met. Entering P always returns to the vehicle setting.",
        "kdT": "TRIGGER (%)", "kdTgt": "Kickdown mode", "kdTgtO": ["Standard", "Sport"],
        "kdRp": "Release when the pedal drops", "kdRpD": "At or below the release level for 0.5 s.",
        "kdR": "RELEASE AT OR BELOW (%)", "kdRb": "Release on brake", "kdRbD": "Any brake press switches back at once.",
        "kdRm": "Switch back to", "kdRmO": ["Comfort (Chill)", "Vehicle setting"],
        "hd": "Door Handle Macro",
        "hdD": "Hold an exterior front door handle to run an action. Default action: open the frunk, only in P. A normal short pull does nothing.",
        "hdDr": "Driver door handle", "hdPs": "Passenger door handle", "hdS": "HOLD (s)", "hdA": "Action",
        "lo": "Listen-Only Mode",
        "loD": "CAN hardware only listens: nothing is sent or acknowledged, so every feature that sends CAN frames pauses. Dashboard, Wi-Fi and Bluetooth keep working. Saved across reboots.",
        "loBusy": "Switching CAN mode…", "loFail": "Mode change not confirmed. Check Diagnostics.",
        "stKd": "Pedal · brake · kickdown", "stHd": "Door handles",
        "rh": "Reverse Hazards", "rhD": "Hazards on while in R, off again when leaving R.",
        "rl": "Reverse Rear Light", "rlD": "Turn the rear lights on while in R.",
        "rlm": ["Rear fog", "Lights ON", "Rear fog + lights ON"], "mode": "Rear light mode",
        "dh": "Door-Open Hazards (D/R)", "dhD": "A door opened in D or R turns the hazards on until every door is closed again.",
        "dd": "OFF DELAY AFTER CLOSE (ms)",
        "save": "Save", "saving": "Saving…", "saved": "Saved", "failed": "Not saved",
        "status": "LIVE STATUS", "foldNow": "Fold / Unfold Mirrors", "camNow": "Camera View",
        "stBus": "Bus", "stGear": "Gear · speed", "stWx": "Weather", "stDoors": "Doors",
        "stWin": "Windows", "stEe": "Easy Entry", "stMir": "Mirrors", "stCam": "Camera",
        "stHaz": "Hazards", "stCnt": "Counters",
        "on": "ON", "off": "OFF", "na": "N/A",
        "wifiLabel": "WI-FI TIMING",
        "wifiSlow": "Wi-Fi Slow Start",
        "wifiSlowD": "Start the access point after a delay. CAN and every other feature start normally.",
        "wifiHide": "Hide Wi-Fi Name",
        "wifiHideD": "Hide the SSID after the access point has been on for the set time.",
        "wifiStart": "START DELAY (s)", "wifiHideAfter": "HIDE AFTER (s)",
        "wifiSave": "Save Wi-Fi Timing",
        "wifiNote": "A hidden network stays reachable from phones that already saved it. Profile setup mode always starts the access point immediately and never hides it. Hiding restarts the access point once.",
    },
    "zh-CN": {
        "back": "返回", "settings": "设置",
        "rowKey": "车身舒适功能",
        "rowDesc": "便捷上车、双闪、后排安全带和方向盘宏。",
        "title": "车身舒适功能",
        "sub": "适用于 HW4 Model Y Juniper 的车身功能。每项功能在启用前均保持关闭。指令是车身 CAN 上实时车辆帧的副本。",
        "unsupported": "需要车身 + 底盘（Model Y Juniper）或 Model Y L。当前车辆配置无法使用这些功能。",
        "experimental": "当前配置不是 Juniper / Model Y L：帧布局未经验证，请将所有功能视为实验性。",
        "ee": "便捷上车",
        "eeD": "在 P 挡时，打开车门会将该车门车窗完全降下。关门后车窗恢复到原来的位置。",
        "doors": ["左前门", "右前门", "左后门", "右后门"],
        "wx": "雨雪天禁止", "wxD": "使用车辆的雨/雪检测信号。",
        "wxs": "天气未知时禁止", "wxsD": "天气信号缺失时不执行。",
        "rd": "恢复延迟 (ms)",
        "mf": "折叠后视镜", "mfD": "便捷上车动作时折叠外后视镜。",
        "cam": "折叠时显示摄像头", "camD": "本功能折叠后视镜期间，在 D/N 挡显示摄像头画面。",
        "uk": "展开车速高于 (km/h)", "uh": "持续 (s)",
        "ufD": "由本功能折叠的后视镜，在 D 挡车速持续高于设定值达到设定时间后展开。您自己折叠的后视镜不会被展开。",
        "sb": "关闭后排安全带提示音", "sbD": "对所选第二排座椅关闭未系安全带提示音。",
        "seats": ["第二排左", "第二排中", "第二排右"],
        "mc": "方向盘宏",
        "mcD": "双击、三击或长按方向盘按键执行动作，也可切换 Nag Killer、TLSSC 等现有功能：8 个按键 × 3 种手势 = 24 个位置。每次按下时按键的原车功能仍会执行。",
        "lp": "长按 (ms)", "gp": "连击间隔 (ms)",
        "btn": ["左滚轮 · 按下", "右滚轮 · 按下", "左滚轮 · 左拨", "左滚轮 · 右拨", "右滚轮 · 左拨", "右滚轮 · 右拨",
                "语音按钮", "摄像头按钮", "驾驶员车门把手", "乘客车门把手"],
        "ges": ["双击", "三击", "长按"],
        "act": ["无", "切换便捷上车", "切换后视镜折叠", "切换后排安全带提示音", "",
                "切换倒车双闪", "切换倒车后灯", "切换开门双闪", "双闪闪烁", "折叠 / 展开后视镜", "摄像头画面",
                "切换 Nag Killer", "切换 ISA 抑制", "切换禁用驾驶员监控", "切换自动转向灯", "切换 TLSSC",
                "切换车门按钮取消", "切换视觉限速控制", "切换连续 AP", "切换电池预热",
                "打开前备箱", "切换仅监听模式", "切换 Kickdown"],
        "actNew": "车身舒适功能", "actOld": "现有功能",
        "notSet": "未设置", "setCount": "已设置 {n} 个", "collapsedHint": "点击按键可编辑其动作。",
        "hb": "双闪闪烁", "hbT": "双闪闪烁（持续 {n} 秒）",
        "hbD": "一次性指令：通过宏或“立即闪烁”开启双闪，持续设定时间后关闭。此开关只允许或阻止该指令，不会让双闪一直亮着。如果双闪已开启则不执行。",
        "hc": "持续时间 (s)", "run": "立即闪烁",
        "stMacro": "最近的宏",
        "kd": "Kickdown",
        "kdD": "在 D 挡把加速踏板踩到触发值时，切换到更强的加速模式（与“踏板响应”相同机制）。满足下方恢复条件后切回。进入 P 挡时总是恢复车辆设置。",
        "kdT": "触发 (%)", "kdTgt": "Kickdown 模式", "kdTgtO": ["标准", "运动"],
        "kdRp": "踏板回落时恢复", "kdRpD": "不高于恢复值持续 0.5 秒。",
        "kdR": "恢复阈值 (%)", "kdRb": "踩刹车时恢复", "kdRbD": "踩下刹车立即切回。",
        "kdRm": "切回到", "kdRmO": ["舒适 (Chill)", "车辆设置"],
        "hd": "车门把手宏",
        "hdD": "按住前门外把手执行动作。默认动作：打开前备箱（仅限 P 挡）。正常短拉不会触发。",
        "hdDr": "驾驶员车门把手", "hdPs": "乘客车门把手", "hdS": "按住 (s)", "hdA": "动作",
        "lo": "仅监听模式",
        "loD": "CAN 硬件只接收：不发送也不应答任何帧，所有发送 CAN 帧的功能暂停。仪表板、Wi-Fi 和蓝牙照常工作。重启后保持。",
        "loBusy": "正在切换 CAN 模式…", "loFail": "模式切换未确认，请查看诊断。",
        "stKd": "踏板 · 刹车 · Kickdown", "stHd": "车门把手",
        "rh": "倒车双闪", "rhD": "R 挡时开启双闪，离开 R 挡后关闭。",
        "rl": "倒车后灯", "rlD": "R 挡时开启后部灯光。",
        "rlm": ["后雾灯", "灯光开启", "后雾灯 + 灯光开启"], "mode": "后灯模式",
        "dh": "开门双闪 (D/R)", "dhD": "在 D 或 R 挡打开车门时开启双闪，直到所有车门关闭。",
        "dd": "关门后熄灭延迟 (ms)",
        "save": "保存", "saving": "正在保存…", "saved": "已保存", "failed": "未保存",
        "status": "实时状态", "foldNow": "折叠 / 展开后视镜", "camNow": "摄像头画面",
        "stBus": "总线", "stGear": "挡位 · 车速", "stWx": "天气", "stDoors": "车门",
        "stWin": "车窗", "stEe": "便捷上车", "stMir": "后视镜", "stCam": "摄像头",
        "stHaz": "双闪", "stCnt": "计数",
        "on": "开", "off": "关", "na": "不可用",
        "wifiLabel": "WI-FI 时序",
        "wifiSlow": "Wi-Fi 延迟启动",
        "wifiSlowD": "延迟启动接入点。CAN 和其他功能照常启动。",
        "wifiHide": "隐藏 Wi-Fi 名称",
        "wifiHideD": "接入点开启达到设定时间后隐藏 SSID。",
        "wifiStart": "启动延迟 (s)", "wifiHideAfter": "隐藏延迟 (s)",
        "wifiSave": "保存 Wi-Fi 时序",
        "wifiNote": "已保存该网络的手机仍可连接隐藏网络。车辆配置设置模式始终立即启动接入点且不会隐藏。隐藏时接入点会重启一次。",
    },
}

BACK_SVG = ('<svg aria-hidden="true" class="backIcon" viewBox="0 0 24 24"><path d="M15 18l-6-6 6-6" '
            'fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" '
            'stroke-width="2"></path></svg>')


def settings_row(t):
    return ('<div class="listrow clickable" data-panel="panelBodyComfort" id="bodyComfortSettingsRow">'
            f'<div><div class="listkey">{t["rowKey"]}</div><div class="listdesc" id="bodyComfortDesc">{t["rowDesc"]}</div></div>'
            '<div class="rowRight"><span class="value" id="bodyComfortRowState">—</span><span class="chev">›</span></div></div>\n')


def panel(t):
    return (f'<section class="panel" id="panelBodyComfort"><div class="panelin"><div class="paneltop">'
            f'<button aria-label="{t["back"]}" class="backbtn">{BACK_SVG}</button><div class="paneltitle">{t["settings"]}</div></div>'
            f'<div class="panelHero">{t["title"]}</div><div class="panelSub">{t["sub"]}</div>'
            '<div id="bcBody"></div></div></section>\n')


def listen_only_row(t):
    return ('<div class="listrow" id="listenOnlyRow"><div><div class="listkey">' + t["lo"] + '</div>'
            '<div class="listdesc">' + t["loD"] + '</div><div class="listdesc" id="listenOnlyState" aria-live="polite"></div></div>'
            '<label class="toggle"><input id="listenOnlyToggle" type="checkbox"/><span class="track"></span></label></div>\n')


def wifi_card(t):
    return (f'<div class="detailSectionLabel">{t["wifiLabel"]}</div>\n'
            '<section class="card" id="wifiTimingCard">'
            f'<div class="settingToggleRow"><div><div class="listkey">{t["wifiSlow"]}</div><div class="listdesc">{t["wifiSlowD"]}</div></div>'
            '<label class="toggle"><input id="wifiSlowToggle" type="checkbox"/><span class="track"></span></label></div>'
            f'<div class="settingToggleRow"><div><div class="listkey">{t["wifiHide"]}</div><div class="listdesc" id="wifiHideDesc">{t["wifiHideD"]}</div></div>'
            '<label class="toggle"><input id="wifiHideToggle" type="checkbox"/><span class="track"></span></label></div>'
            f'<div class="fieldgrid"><div class="field"><label>{t["wifiStart"]}</label><input id="wifiStartDelay" inputmode="numeric" max="600" min="0" step="1" type="number"/></div>'
            f'<div class="field"><label>{t["wifiHideAfter"]}</label><input id="wifiHideDelay" inputmode="numeric" max="3600" min="0" step="1" type="number"/></div></div>'
            f'<div class="controlgrid"><button class="btn primary" id="wifiTimingApply" style="grid-column:1/-1">{t["wifiSave"]}</button></div></section>\n'
            f'<div class="softNote" id="wifiTimingNote">{t["wifiNote"]}</div>\n')


SCRIPT = r'''<script>(()=>{
const T=__TEXT__;
const $b=id=>document.getElementById(id);
const esc=s=>String(s??'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
let bc=null,bcBusy=0,wt=null;
const tg=(id,key,desc)=>`<div class="settingToggleRow"><div><div class="listkey">${esc(key)}</div>${desc?`<div class="listdesc">${esc(desc)}</div>`:''}</div><label class="toggle"><input id="${id}" type="checkbox"/><span class="track"></span></label></div>`;
const num=(id,label,min,max)=>`<div class="field"><label>${esc(label)}</label><input id="${id}" inputmode="numeric" max="${max}" min="${min}" step="1" type="number"/></div>`;
const sel=(id,label,opts,style)=>`<select aria-label="${esc(label)}" class="selectArrow labSelect" id="${id}"${style?` style="${style}"`:''}>${opts.map((o,i)=>`<option value="${i}">${esc(o)}</option>`).join('')}</select>`;
const NEW_ACTS=[0,1,2,3,5,6,7,22,8,9,10,20],OLD_ACTS=[11,12,13,14,15,16,17,18,19,21];
const opt=i=>`<option value="${i}">${esc(T.act[i])}</option>`;
const macroSel=(id,label)=>`<select aria-label="${esc(label)}" class="selectArrow labSelect" id="${id}" style="max-width:62%">${opt(0)}<optgroup label="${esc(T.actNew)}">${NEW_ACTS.slice(1).map(opt).join('')}</optgroup><optgroup label="${esc(T.actOld)}">${OLD_ACTS.map(opt).join('')}</optgroup></select>`;
const MG_CSS=`<style>.bcMgHead{display:flex;align-items:center;gap:10px;width:100%;padding:12px 16px;background:none;border:0;border-top:1px solid var(--line);color:inherit;font:inherit;text-align:left;cursor:pointer}.bcMgHead:focus-visible{outline:2px solid var(--field-ink);outline-offset:-2px}.bcMgText{flex:1;min-width:0;display:flex;flex-direction:column;gap:3px}.bcMgName{font-weight:650;font-size:14px}.bcMgSum{font-size:11.5px;line-height:1.4;opacity:.62;overflow-wrap:anywhere}.bcMg.set .bcMgSum{opacity:1;font-weight:600}.bcMgBadge{flex:0 0 auto;min-width:22px;height:22px;padding:0 7px;border-radius:11px;background:var(--field-ink);color:var(--field);font-size:11px;font-weight:700;display:inline-flex;align-items:center;justify-content:center}.bcMgBadge[hidden],.bcMgBody[hidden]{display:none}.bcMgChev{flex:0 0 auto;transition:transform .2s}.bcMgHead[aria-expanded="true"] .bcMgChev{transform:rotate(90deg)}@media (prefers-reduced-motion:reduce){.bcMgChev{transition:none}}</style>`;
const card=(inner,id)=>`<section class="card"${id?` id="${id}"`:''}>${inner}</section>`;
const lbl=s=>`<div class="detailSectionLabel">${esc(s)}</div>`;
const btnRow=(...b)=>`<div class="controlgrid">${b.map(([id,txt,primary])=>`<button class="btn${primary?' primary':''}" id="${id}"${b.length===1?' style="grid-column:1/-1"':''}>${esc(txt)}</button>`).join('')}</div>`;
const stRow=(id,key)=>`<div class="detailRow"><div><div class="detailLeft">${esc(key)}</div></div><span class="detailValue" id="${id}">—</span></div>`;
function build(){
  const root=$b('bcBody');if(!root)return;
  const h=[];
  h.push(`<div class="softNote" id="bcSupport" hidden></div>`);
  h.push(lbl(T.ee),card(tg('bcEe',T.ee,T.eeD)+T.doors.map((d,i)=>tg('bcDoor'+i,d,'')).join('')+tg('bcWx',T.wx,T.wxD)+tg('bcWxs',T.wxs,T.wxsD)+`<div class="fieldgrid">${num('bcRd',T.rd,0,5000)}</div>`+tg('bcMf',T.mf,T.mfD)+tg('bcCam',T.cam,T.camD)+`<div class="fieldgrid">${num('bcUk',T.uk,1,80)}${num('bcUh',T.uh,0,30)}</div><div class="softNote">${esc(T.ufD)}</div>`+btnRow(['bcEeSave',T.save,1])));
  h.push(lbl(T.sb),card(tg('bcSb',T.sb,T.sbD)+T.seats.map((s,i)=>tg('bcSeat'+i,s,'')).join('')));
  let m=tg('bcMc',T.mc,T.mcD)+`<div class="fieldgrid">${num('bcLp',T.lp,500,2000)}${num('bcGp',T.gp,200,800)}</div>`+btnRow(['bcMcSave',T.save,1]);
  m+=`<div class="softNote" style="padding:4px 16px 8px">${esc(T.collapsedHint)}</div>`;
  [6,7,0,1,2,3,4,5].forEach(bi=>{const b=T.btn[bi];m+=`<div class="bcMg" id="bcMg${bi}"><button type="button" class="bcMgHead" aria-expanded="false" aria-controls="bcMgB${bi}"><span class="bcMgText"><span class="bcMgName">${esc(b)}</span><span class="bcMgSum" id="bcMgS${bi}">${esc(T.notSet)}</span></span><span class="bcMgBadge" id="bcMgN${bi}" hidden></span><span class="chev bcMgChev" aria-hidden="true">›</span></button><div class="bcMgBody" id="bcMgB${bi}" hidden>`+T.ges.map((g,gi)=>`<div class="listrow"><div><div class="listdesc">${esc(g)}</div></div>${macroSel(`bcM${bi}_${gi}`,b+' · '+g)}</div>`).join('')+`</div></div>`});
  h.push(MG_CSS,`<div class="detailSectionLabel" id="bcMcLabel">${esc(T.mc)}</div>`,card(m));
  h.push(`<div class="detailSectionLabel" id="bcHbLabel">${esc(T.hb)}</div>`,card(tg('bcHb',T.hb,T.hbD)+`<div class="fieldgrid">${num('bcHc',T.hc,1,30)}</div>`+btnRow(['bcHbSave',T.save,1],['bcHbRun',T.run,0])));
  h.push(lbl(T.rh),card(tg('bcRh',T.rh,T.rhD)));
  h.push(lbl(T.rl),card(tg('bcRl',T.rl,T.rlD)+`<div class="listrow"><div><div class="listkey">${esc(T.mode)}</div></div>${sel('bcRlm',T.mode,T.rlm,'max-width:62%')}</div>`));
  h.push(lbl(T.dh),card(tg('bcDh',T.dh,T.dhD)+`<div class="fieldgrid">${num('bcDd',T.dd,0,10000)}</div>`+btnRow(['bcDhSave',T.save,1])));
  h.push(lbl(T.kd),card(tg('bcKd',T.kd,T.kdD)+`<div class="listrow"><div><div class="listkey">${esc(T.kdTgt)}</div></div>${sel('bcKdTgt',T.kdTgt,T.kdTgtO,'max-width:62%')}</div>`+`<div class="fieldgrid">${num('bcKdT',T.kdT,10,100)}</div>`+tg('bcKdRp',T.kdRp,T.kdRpD)+`<div class="fieldgrid">${num('bcKdR',T.kdR,0,90)}</div>`+tg('bcKdRb',T.kdRb,T.kdRbD)+`<div class="listrow"><div><div class="listkey">${esc(T.kdRm)}</div></div>${sel('bcKdRm',T.kdRm,T.kdRmO,'max-width:62%')}</div>`+btnRow(['bcKdSave',T.save,1])));
  h.push(lbl(T.hd),card(tg('bcHd',T.hd,T.hdD)+tg('bcHd0',T.hdDr,'')+tg('bcHd1',T.hdPs,'')+`<div class="fieldgrid">${num('bcHdS',T.hdS,1,10)}</div>`+btnRow(['bcHdSave',T.save,1])+`<div class="listrow"><div><div class="listkey">${esc(T.hdA)}</div></div>${macroSel('bcHdA',T.hdA)}</div>`));
  h.push(lbl(T.status),card(stRow('bcStBus',T.stBus)+stRow('bcStGear',T.stGear)+stRow('bcStWx',T.stWx)+stRow('bcStDoors',T.stDoors)+stRow('bcStWin',T.stWin)+stRow('bcStEe',T.stEe)+stRow('bcStMir',T.stMir)+stRow('bcStCam',T.stCam)+stRow('bcStHaz',T.stHaz)+stRow('bcStKd',T.stKd)+stRow('bcStHd',T.stHd)+stRow('bcStMacro',T.stMacro)+stRow('bcStCnt',T.stCnt)+btnRow(['bcFoldNow',T.foldNow,0],['bcCamNow',T.camNow,0])));
  h.push(`<div class="softNote" id="bcSaveState" aria-live="polite"></div>`);
  root.innerHTML=h.join('');
}
build();
const setText=(id,v)=>{const e=$b(id);if(e)e.textContent=v};
const setChk=(id,v)=>{const e=$b(id);if(e&&!e.dataset.busy)e.checked=!!v};
const setVal=(id,v)=>{const e=$b(id);if(e&&document.activeElement!==e&&!e.dataset.dirty)e.value=String(v)};
function render(d){
  if(!d||!d.cfg)return;bc=d;const c=d.cfg,l=d.live||{},s=d.stats||{};
  const sup=$b('bcSupport');if(sup){sup.hidden=d.supported&&d.layout!=='EXPERIMENTAL';sup.textContent=d.supported?T.experimental:T.unsupported}
  $b('bcBody')?.querySelectorAll('input,select,button').forEach(e=>{e.disabled=!d.supported});
  setChk('bcEe',c.eeEnabled);for(let i=0;i<4;i++)setChk('bcDoor'+i,(c.eeDoorMask>>i)&1);
  setChk('bcWx',c.eeWeatherBlock);setChk('bcWxs',c.eeWeatherStrict);setVal('bcRd',c.eeRestoreDelayMs);
  setChk('bcMf',c.eeMirrorFold);setChk('bcCam',c.eeMirrorCamera);setVal('bcUk',c.eeUnfoldKph);setVal('bcUh',c.eeUnfoldHoldS);
  setChk('bcSb',c.sbEnabled);for(let i=0;i<3;i++)setChk('bcSeat'+i,(c.sbSeatMask>>i)&1);
  setChk('bcMc',c.mcEnabled);setVal('bcLp',c.mcLongMs);setVal('bcGp',c.mcGapMs);
  (c.mcAction||[]).forEach((row,bi)=>row.forEach((a,gi)=>setVal(`bcM${bi}_${gi}`,a)));
  renderMacroSummary(c);
  const hbT=T.hbT.replace('{n}',c.hzBlinkSeconds);setText('bcHbLabel',hbT);const hbKey=$b('bcHb')?.closest('.settingToggleRow')?.querySelector('.listkey');if(hbKey)hbKey.textContent=hbT;
  setChk('bcHb',c.hzBlinkEnabled);setVal('bcHc',c.hzBlinkSeconds);setChk('bcRh',c.rvHazard);
  setChk('bcRl',c.rvLight);setVal('bcRlm',c.rvLightMode);setChk('bcDh',c.drHazard);setVal('bcDd',c.drOffDelayMs);
  setChk('bcKd',c.kdEnabled);setVal('bcKdTgt',c.kdTargetRaw===2?1:0);setVal('bcKdT',c.kdTriggerPct);setChk('bcKdRp',c.kdReleaseOnPedal);setVal('bcKdR',c.kdReleasePct);setChk('bcKdRb',c.kdReleaseOnBrake);setVal('bcKdRm',c.kdReleaseMode);
  setChk('bcHd',c.dhEnabled);setChk('bcHd0',c.dhDoorMask&1);setChk('bcHd1',(c.dhDoorMask>>1)&1);setVal('bcHdS',c.dhHoldS);setVal('bcHdA',c.dhAction);
  setText('bcStKd',`${l.pedalPct==null?'—':l.pedalPct+' %'} · ${l.brake||'?'} · ${l.kickdown||'—'}`);setText('bcStHd',l.handles||'—');
  if(typeof l.listenOnly==='boolean')window.__setListenOnly?.(l.listenOnly);
  setText('bcStBus',d.bus+(d.layout?' · '+d.layout:''));
  setText('bcStGear',(l.gear||'?')+' · '+(l.speedKph==null?'—':l.speedKph+' km/h'));
  setText('bcStWx',l.weather||'—');setText('bcStDoors',l.doors||'—');setText('bcStWin',l.windows||'—');
  setText('bcStEe',l.easyEntry||'—');setText('bcStMir',(l.mirrors||'—')+(l.mirrorFoldedByFeature?' · FEATURE':''));
  setText('bcStCam',l.camera||'—');
  setText('bcStHaz',(l.hazard||'?')+(l.hazardOwned?' · FEATURE':'')+(l.blinkActive?' · BLINK':'')+(l.reverseHazard?' · R':'')+(l.doorHazard?' · DOOR':'')+(l.rearLight?' · REAR LIGHT':''));
  setText('bcStMacro',s.macroEvents?`${T.btn[s.lastMacroButton]||'?'} · ${T.ges[s.lastMacroGesture]||'?'} → ${T.act[s.lastMacroAction]||T.notSet} · ${s.lastMacroResult||'—'}`:'—');
  setText('bcStCnt',`TX ${s.txOk||0}/${s.txFail||0} · EE ${s.entryOpens||0}/${s.entryRestores||0} · WX ${s.entryBlockedWeather||0} · BELT ${s.seatbeltInjected||0} · MACRO ${s.macroEvents||0}`);
  const n=[c.eeEnabled,c.sbEnabled,c.mcEnabled,c.hzBlinkEnabled,c.rvHazard,c.rvLight,c.drHazard].filter(Boolean).length;
  setText('bodyComfortRowState',!d.supported?T.na:n?n+' '+T.on:T.off);
}
function renderMacroSummary(c){
  let total=0;
  for(let bi=0;bi<8;bi++){
    const row=(c.mcAction||[])[bi]||[0,0,0],parts=[];
    row.forEach((a,gi)=>{if(a&&T.act[a])parts.push(`${T.ges[gi]} → ${T.act[a]}`)});
    total+=parts.length;
    const g=$b('bcMg'+bi);if(g)g.classList.toggle('set',parts.length>0);
    setText('bcMgS'+bi,parts.length?parts.join(' · '):T.notSet);
    const n=$b('bcMgN'+bi);if(n){n.hidden=!parts.length;n.textContent=String(parts.length)}
  }
  setText('bcMcLabel',total?`${T.mc} · ${T.setCount.replace('{n}',total)}`:T.mc);
}
$b('bcBody')?.addEventListener('click',e=>{const head=e.target.closest('.bcMgHead');if(!head)return;const body=$b(head.getAttribute('aria-controls'));const open=head.getAttribute('aria-expanded')!=='true';head.setAttribute('aria-expanded',String(open));if(body)body.hidden=!open});
async function fetchBody(){try{render(await jget('/api/body/config'))}catch(e){}}
window.fetchBodyComfort=fetchBody;
async function save(params,el){
  if(el)el.dataset.busy='1';bcBusy++;setText('bcSaveState',T.saving);
  try{const d=await featurePostJson('/api/body/update?'+new URLSearchParams(params));if(el)delete el.dataset.busy;render(d);setText('bcSaveState',T.saved);return true}
  catch(e){if(el){delete el.dataset.busy;if(el.type==='checkbox')el.checked=!el.checked}setText('bcSaveState',T.failed+' · '+(e.message||e));fetchBody();return false}
  finally{bcBusy--}
}
const bindToggle=(id,key)=>{const e=$b(id);if(e)e.onchange=()=>save({[key]:e.checked?1:0},e)};
[['bcEe','eeEnabled'],['bcWx','eeWeatherBlock'],['bcWxs','eeWeatherStrict'],['bcMf','eeMirrorFold'],['bcCam','eeMirrorCamera'],['bcSb','sbEnabled'],['bcMc','mcEnabled'],['bcHb','hzBlinkEnabled'],['bcRh','rvHazard'],['bcRl','rvLight'],['bcDh','drHazard'],['bcKd','kdEnabled'],['bcKdRp','kdReleaseOnPedal'],['bcKdRb','kdReleaseOnBrake'],['bcHd','dhEnabled']].forEach(([id,k])=>bindToggle(id,k));
const maskOf=(prefix,n)=>{let m=0;for(let i=0;i<n;i++)if($b(prefix+i)?.checked)m|=1<<i;return m};
for(let i=0;i<4;i++){const e=$b('bcDoor'+i);if(e)e.onchange=()=>save({eeDoorMask:maskOf('bcDoor',4)},e)}
for(let i=0;i<3;i++){const e=$b('bcSeat'+i);if(e)e.onchange=()=>save({sbSeatMask:maskOf('bcSeat',3)},e)}
for(let bi=0;bi<8;bi++)for(let gi=0;gi<3;gi++){const e=$b(`bcM${bi}_${gi}`);if(e)e.onchange=()=>save({mc:`${bi},${gi},${e.value}`})}
const rlm=$b('bcRlm');if(rlm)rlm.onchange=()=>save({rvLightMode:rlm.value});
$b('bcBody')?.addEventListener('input',e=>{if(e.target.type==='number')e.target.dataset.dirty='1'});
function readNums(map){const out={};for(const[id,key]of map){const e=$b(id),v=Number(e?.value);if(!e||e.value===''||!Number.isInteger(v)||v<Number(e.min)||v>Number(e.max)){e?.focus();setText('bcSaveState',T.failed+' · '+key);return null}out[key]=v}return out}
async function saveNums(map){const p=readNums(map);if(!p)return;if(await save(p))map.forEach(([id])=>{const e=$b(id);if(e)delete e.dataset.dirty});}
const onClick=(id,fn)=>{const e=$b(id);if(e)e.onclick=fn};
onClick('bcEeSave',()=>saveNums([['bcRd','eeRestoreDelayMs'],['bcUk','eeUnfoldKph'],['bcUh','eeUnfoldHoldS']]));
onClick('bcMcSave',()=>saveNums([['bcLp','mcLongMs'],['bcGp','mcGapMs']]));
onClick('bcHbSave',()=>saveNums([['bcHc','hzBlinkSeconds']]));
onClick('bcDhSave',()=>saveNums([['bcDd','drOffDelayMs']]));
onClick('bcKdSave',()=>saveNums([['bcKdT','kdTriggerPct'],['bcKdR','kdReleasePct']]));
onClick('bcHdSave',()=>saveNums([['bcHdS','dhHoldS']]));
for(let i=0;i<2;i++){const e=$b('bcHd'+i);if(e)e.onchange=()=>save({dhDoorMask:maskOf('bcHd',2)},e)}
const kdTgt=$b('bcKdTgt');if(kdTgt)kdTgt.onchange=()=>save({kdTargetRaw:kdTgt.value==='1'?2:1});
const kdRm=$b('bcKdRm');if(kdRm)kdRm.onchange=()=>save({kdReleaseMode:kdRm.value});
const hdA=$b('bcHdA');if(hdA)hdA.onchange=()=>save({dhAction:hdA.value});
// Listen-only (Settings > System).
const lo=$b('listenOnlyToggle');let loBusy=false;
window.__setListenOnly=v=>{if(lo&&!loBusy)lo.checked=!!v};
async function fetchListenOnly(){try{const d=await jget('/api/system/listen-only');window.__setListenOnly(d.listenOnly)}catch(e){}}
if(lo)lo.onchange=async()=>{const want=lo.checked;loBusy=true;lo.disabled=true;setText('listenOnlyState',T.loBusy);
  try{const d=await featurePostJson('/api/system/listen-only?enabled='+(want?1:0));loBusy=false;window.__setListenOnly(d.listenOnly);setText('listenOnlyState','')}
  catch(e){loBusy=false;lo.checked=!want;setText('listenOnlyState',T.loFail);fetchListenOnly()}
  finally{lo.disabled=false}};
setTimeout(fetchListenOnly,1800);
async function action(id){try{render(await featurePostJson('/api/body/action?id='+id));setText('bcSaveState','')}catch(e){setText('bcSaveState',T.failed+' · '+(e.message||e))}}
onClick('bcHbRun',()=>action(8));onClick('bcFoldNow',()=>action(9));onClick('bcCamNow',()=>action(10));
// Wi-Fi timing (Wireless panel).
function renderWifi(d){if(!d||d.ok===false)return;wt=d;setChk('wifiSlowToggle',d.slowStartEnabled);setChk('wifiHideToggle',d.hideEnabled);setVal('wifiStartDelay',d.startDelayS);setVal('wifiHideDelay',d.hideDelayS);const hd=$b('wifiHideDesc');if(hd)hd.textContent=T.wifiHideD+(d.hiddenNow?' · '+T.on:'')}
async function fetchWifiTiming(){try{renderWifi(await jget('/api/wifi/timing'))}catch(e){}}
window.fetchWifiTiming=fetchWifiTiming;
async function saveWifi(params,el){if(el)el.dataset.busy='1';try{const d=await featurePostJson('/api/wifi/timing?'+new URLSearchParams(params));if(el)delete el.dataset.busy;if('startDelayS'in params)['wifiStartDelay','wifiHideDelay'].forEach(i=>{const e=$b(i);if(e)delete e.dataset.dirty});renderWifi(d)}catch(e){if(el){delete el.dataset.busy;if(el.type==='checkbox')el.checked=!el.checked}fetchWifiTiming()}}
['wifiStartDelay','wifiHideDelay'].forEach(i=>{const e=$b(i);if(e)e.oninput=()=>{e.dataset.dirty='1'}});
const ws=$b('wifiSlowToggle');if(ws)ws.onchange=()=>saveWifi({slowStartEnabled:ws.checked?1:0},ws);
const wh=$b('wifiHideToggle');if(wh)wh.onchange=()=>saveWifi({hideEnabled:wh.checked?1:0},wh);
onClick('wifiTimingApply',()=>{const a=Number($b('wifiStartDelay')?.value),b=Number($b('wifiHideDelay')?.value);if(!Number.isInteger(a)||a<0||a>600||!Number.isInteger(b)||b<0||b>3600)return;saveWifi({slowStartEnabled:$b('wifiSlowToggle')?.checked?1:0,startDelayS:a,hideEnabled:$b('wifiHideToggle')?.checked?1:0,hideDelayS:b})});
setTimeout(fetchBody,1500);
})();</script>
'''


def patch(path: Path, lang: str) -> bool:
    t = TEXT[lang]
    src = path.read_text(encoding="utf-8")
    if MARKER in src:
        print(f"{path}: already patched")
        return False
    lines = src.split("\n")

    def find(pred, what):
        hits = [i for i, l in enumerate(lines) if pred(l)]
        if len(hits) != 1:
            sys.exit(f"{path}: anchor {what!r} found {len(hits)} times")
        return hits[0]

    # 1) Settings row before the S3XY feature row.
    i = find(lambda l: 'id="s3xyFeatureDesc"' in l and l.startswith('<div class="listrow">'), "s3xy row")
    lines.insert(i, settings_row(t).rstrip("\n"))
    # 2) Wi-Fi timing card after the Wi-Fi password note.
    i = find(lambda l: l.startswith('<div class="softNote" id="wifiPasswordHint">'), "wifi hint")
    lines.insert(i + 1, wifi_card(t).rstrip("\n"))
    # 2b) Listen-only toggle in the System card, after the Wireless row.
    i = find(lambda l: l.startswith('<div class="listrow clickable" data-panel="panelWireless">'), "wireless row")
    lines.insert(i + 1, listen_only_row(t).rstrip("\n"))
    # 3) Panel before the settings detail panels marker.
    i = find(lambda l: l.strip() == "<!-- Settings detail panels -->", "panels marker")
    lines.insert(i, panel(t).rstrip("\n"))
    # 4) Polling.
    i = find(lambda l: l.startswith("const pollLast={"), "pollLast")
    assert "preheat:0," in lines[i]
    lines[i] = lines[i].replace("preheat:0,", "preheat:0,body:0,wifiTiming:0,", 1)
    i = find(lambda l: "else if(p==='panelBatteryPreheat')pollDue('preheat'" in l, "preheat poll")
    lines.insert(i + 1, "    else if(p==='panelBodyComfort')pollDue('body',1000,()=>window.fetchBodyComfort?.(),now,force);")
    i = find(lambda l: "else if(p==='panelWireless')pollDue('s3',1000,fetchS3,now,force);" in l, "wireless poll")
    lines[i] = lines[i].replace(
        "else if(p==='panelWireless')pollDue('s3',1000,fetchS3,now,force);",
        "else if(p==='panelWireless'){pollDue('s3',1000,fetchS3,now,force);pollDue('wifiTiming',3000,()=>window.fetchWifiTiming?.(),now,force)}")
    # 5) Script before the select-sheet enhancer so generated <select>s get it.
    i = find(lambda l: "sheet=q('selectSheet')" in l, "select sheet script")
    assert lines[i - 1] == "<script>(()=>{", lines[i - 1]
    script = SCRIPT.replace("__TEXT__", json.dumps(t, ensure_ascii=False, separators=(",", ":")))
    lines.insert(i - 1, script.rstrip("\n"))
    path.write_text("\n".join(lines), encoding="utf-8")
    print(f"{path}: patched")
    return True


if __name__ == "__main__":
    patch(ROOT / "dashboard_source.html", "en")
    patch(ROOT / "zh-CN" / "dashboard_source_zh-CN.html", "zh-CN")
