#!/usr/bin/env python3
"""Build an offline preview of the Automation panel (HW4 Model Y Juniper,
Body + Chassis) on top of the Body Comfort preview and its car simulator.

Usage: python3 tools/build_automation_preview.py [output.html]
Default output: docs/automation-preview.html
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_body_comfort_preview as bcp  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "automation-preview.html"
    html = bcp.build_html()
    if 'id="panelAutomation"' not in html:
        raise SystemExit("dashboard_source.html has no Automation panel; run tools/add_automation_dashboard.py")
    html = html.replace("Body Comfort Preview · ", "Automation Preview · ", 1)
    opener = "openPanel('panelBodyComfort')}catch(e){}},1200)"
    assert html.count(opener) == 1, "preview opener changed"
    html = html.replace(opener, "openPanel('panelAutomation')}catch(e){}},1200)", 1)
    button = '<button id="bcSimOpen" type="button">Body Comfort 열기</button>'
    assert button in html
    html = html.replace(button, '<button id="autoSimOpen" type="button">Automation 열기</button>' + button, 1)
    html = html.replace("</body>", "<script>document.getElementById('autoSimOpen').onclick=()=>{try{showPage('settings');openPanel('panelAutomation')}catch(e){}document.getElementById('bcSim').classList.remove('show')};</script>\n</body>", 1)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(html, encoding="utf-8")
    print(f"preview={out} bytes={out.stat().st_size}")


if __name__ == "__main__":
    main()
