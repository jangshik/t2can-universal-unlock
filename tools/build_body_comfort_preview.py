#!/usr/bin/env python3
"""Build an offline preview of the Body Comfort features (HW4 Model Y Juniper,
Body + Chassis) from the current production dashboard source.

Usage: python3 tools/build_body_comfort_preview.py [output.html]
Default output: docs/body-comfort-preview.html
"""
import base64
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SIM_CSS = r"""
#bcSimFab{position:fixed;left:12px;bottom:calc(env(safe-area-inset-bottom,0px) + 86px);z-index:120;border:0;border-radius:22px;padding:10px 14px;font:600 13px -apple-system,system-ui,sans-serif;background:#2f6fed;color:#fff;box-shadow:0 6px 18px rgba(0,0,0,.25)}
#bcSim{position:fixed;left:8px;right:8px;bottom:calc(env(safe-area-inset-bottom,0px) + 136px);max-width:504px;margin:0 auto;max-height:62vh;overflow:auto;z-index:121;border-radius:18px;padding:12px 12px 14px;background:#1d1f23;color:#eef0f3;font:13px -apple-system,system-ui,sans-serif;box-shadow:0 10px 30px rgba(0,0,0,.35);display:none}
#bcSim.show{display:block}
#bcSim h4{margin:10px 0 6px;font-size:11px;letter-spacing:.06em;text-transform:uppercase;color:#9aa0aa}
#bcSim h4:first-child{margin-top:0}
#bcSim .row{display:flex;flex-wrap:wrap;gap:6px;align-items:center}
#bcSim button{border:1px solid #3a3d44;background:#2a2d33;color:#eef0f3;border-radius:14px;padding:7px 10px;font:600 12px -apple-system,system-ui,sans-serif}
#bcSim button.on{background:#2f6fed;border-color:#2f6fed}
#bcSim select{background:#2a2d33;color:#eef0f3;border:1px solid #3a3d44;border-radius:12px;padding:6px;font-size:12px;max-width:100%}
#bcSim input[type=range]{flex:1;min-width:120px}
#bcSim .note{color:#9aa0aa;font-size:11px;line-height:1.4;margin-top:6px}
#bcSimLog{font:11px ui-monospace,SFMono-Regular,Menlo,monospace;color:#b9f6ca;white-space:pre-wrap;background:#121316;border-radius:10px;padding:8px;min-height:40px;margin-top:4px}
"""

SIM_HTML = r"""
<button id="bcSimFab" type="button">🚗 Juniper 시뮬레이터</button>
<div id="bcSim" role="dialog" aria-label="Vehicle simulator">
  <div class="row"><button id="bcSimOpen" type="button">Body Comfort 열기</button><button id="bcSimWifi" type="button">Wi-Fi 타이밍 열기</button><button id="bcSimClose" type="button">닫기</button></div>
  <h4>기어 · 속도</h4>
  <div class="row" id="bcSimGear"></div>
  <div class="row"><input id="bcSimSpeed" type="range" min="0" max="60" step="1" value="0"><span id="bcSimSpeedV">0 km/h</span></div>
  <h4>문 (탭하면 열기/닫기)</h4>
  <div class="row" id="bcSimDoors"></div>
  <h4>날씨</h4>
  <div class="row"><button id="bcSimRain" type="button">비</button><button id="bcSimSnow" type="button">눈</button></div>
  <h4>2열 승객 탑승 · 벨트 미착용</h4>
  <div class="row" id="bcSimBelts"></div>
  <h4>핸들 버튼 제스처</h4>
  <div class="row"><select id="bcSimBtn"></select></div>
  <div class="row" style="margin-top:6px"><button data-g="0" type="button">더블</button><button data-g="1" type="button">트리플</button><button data-g="2" type="button">롱</button><button id="bcSimHaz" type="button">운전자 비상등 버튼</button></div>
  <h4>차량 상태 / 송신 로그</h4>
  <div id="bcSimState" class="note"></div>
  <div id="bcSimLog"></div>
  <div class="note">미리보기용 가상 차량입니다. 실제 펌웨어 로직(body_comfort_runtime.h)을 단순화해 흉내 냅니다.</div>
</div>
"""

SIM_JS = r"""
(()=>{
  const P=window.__bcPreview,$=id=>document.getElementById(id),car=P.car;
  const BTN=['왼쪽 스크롤 누름','오른쪽 스크롤 누름','왼쪽 스크롤 좌틸트','왼쪽 스크롤 우틸트','오른쪽 스크롤 좌틸트','오른쪽 스크롤 우틸트'];
  const DOORS=[['FL','운전석'],['FR','조수석'],['RL','뒷좌석 좌'],['RR','뒷좌석 우']];
  $('bcSimFab').onclick=()=>$('bcSim').classList.toggle('show');
  $('bcSimClose').onclick=()=>$('bcSim').classList.remove('show');
  const go=(panel)=>{try{showPage('settings');openPanel(panel)}catch(e){}$('bcSim').classList.remove('show')};
  $('bcSimOpen').onclick=()=>go('panelBodyComfort');
  $('bcSimWifi').onclick=()=>go('panelWireless');
  $('bcSimGear').innerHTML=['P','R','N','D'].map(g=>`<button type="button" data-gear="${g}">${g}</button>`).join('');
  $('bcSimGear').querySelectorAll('button').forEach(b=>b.onclick=()=>{car.gear=b.dataset.gear;if(car.gear==='P')car.kph=0;P.log('Gear → '+car.gear)});
  $('bcSimSpeed').oninput=e=>{car.kph=Number(e.target.value);if(car.kph>0&&car.gear==='P')car.gear='D'};
  $('bcSimDoors').innerHTML=DOORS.map(([k,n],i)=>`<button type="button" data-door="${i}">${n}</button>`).join('');
  $('bcSimDoors').querySelectorAll('button').forEach(b=>b.onclick=()=>{const i=Number(b.dataset.door);car.door[i]=!car.door[i];P.log(`Door ${DOORS[i][0]} ${car.door[i]?'opened':'closed'}`);car.door[i]?P.doorOpened(i):P.doorClosed(i)});
  $('bcSimRain').onclick=()=>{car.rain=!car.rain;P.log('Rain '+(car.rain?'ON':'OFF'))};
  $('bcSimSnow').onclick=()=>{car.snow=!car.snow;P.log('Snow '+(car.snow?'ON':'OFF'))};
  $('bcSimBelts').innerHTML=['좌','중','우'].map((n,i)=>`<button type="button" data-seat="${i}">${n}</button>`).join('');
  $('bcSimBelts').querySelectorAll('button').forEach(b=>b.onclick=()=>{const i=Number(b.dataset.seat);car.belts[i]=!car.belts[i]});
  $('bcSimBtn').innerHTML=BTN.map((n,i)=>`<option value="${i}">${n}</option>`).join('');
  document.querySelectorAll('#bcSim [data-g]').forEach(b=>b.onclick=()=>{P.log(`Wheel: ${BTN[$('bcSimBtn').value]} · ${b.textContent}`);P.macro(Number($('bcSimBtn').value),Number(b.dataset.g))});
  $('bcSimHaz').onclick=()=>P.userHazard();
  window.__bcPreviewRender=()=>{
    $('bcSimGear').querySelectorAll('button').forEach(b=>b.classList.toggle('on',b.dataset.gear===car.gear));
    $('bcSimSpeed').value=String(car.kph);$('bcSimSpeedV').textContent=car.kph+' km/h';
    $('bcSimDoors').querySelectorAll('button').forEach(b=>b.classList.toggle('on',car.door[Number(b.dataset.door)]));
    $('bcSimBelts').querySelectorAll('button').forEach(b=>b.classList.toggle('on',car.belts[Number(b.dataset.seat)]));
    $('bcSimRain').classList.toggle('on',car.rain);$('bcSimSnow').classList.toggle('on',car.snow);
    const chime=car.belts.map((u,i)=>u&&!(P.cfg.sbEnabled&&((P.cfg.sbSeatMask>>i)&1))).some(Boolean);
    $('bcSimState').textContent=`창문 ${DOORS.map(([k],i)=>k+' '+car.win[i]+'%').join(' · ')} | 미러 ${car.mirrors} | 카메라 ${car.camera?'ON':'OFF'} | 비상등 ${car.hazard}${P.st.rearLight?' | 후미등 ON':''} | 벨트 알람 ${chime?'울림':'없음'}`;
    $('bcSimLog').textContent=car.log.join('\n');
  };
  window.__bcPreviewRender();
  if(new URLSearchParams(location.search).get('open')!=='none')setTimeout(()=>{try{showPage('settings');openPanel('panelBodyComfort')}catch(e){}},1200);
})();
"""


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "body-comfort-preview.html"
    html = (ROOT / "dashboard_source.html").read_text(encoding="utf-8")
    if 'id="panelBodyComfort"' not in html:
        raise SystemExit("dashboard_source.html has no Body Comfort panel; run tools/add_body_comfort_dashboard.py")
    for filename, url in (("geist-latin.woff2", "/fonts/geist-lab.woff2"),
                          ("geist-mono-latin.woff2", "/fonts/geist-mono-lab.woff2")):
        data = base64.b64encode((ROOT / "fonts" / filename).read_bytes()).decode("ascii")
        html = html.replace(url, "data:font/woff2;base64," + data)
    base_mock = (ROOT / "tools" / "v38_preview_mock.js").read_text(encoding="utf-8")
    default_model = "const model=[1,2,3,4,5].includes(requestedModel)?requestedModel:1;"
    if default_model not in base_mock:
        raise SystemExit("v38_preview_mock.js changed; update the Juniper default")
    base_mock = base_mock.replace(default_model, default_model[:-2] + "2;")  # Juniper, Body + Chassis
    body_mock = (ROOT / "tools" / "body_comfort_preview_mock.js").read_text(encoding="utf-8")
    html = html.replace("<title>", "<title>Body Comfort Preview · ", 1) if "<title>" in html else html
    html = html.replace("</head>",
                        "<style id=\"bc-preview-style\">" + SIM_CSS + "</style>\n"
                        "<script id=\"bc-preview-base-mock\">\n" + base_mock + "</script>\n"
                        "<script id=\"bc-preview-body-mock\">\n" + body_mock + "</script>\n</head>", 1)
    html = html.replace("</body>", SIM_HTML + "<script id=\"bc-preview-sim\">" + SIM_JS + "</script>\n</body>", 1)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(html, encoding="utf-8")
    print(f"preview={out} bytes={out.stat().st_size}")


if __name__ == "__main__":
    main()
