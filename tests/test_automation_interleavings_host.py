"""Run each production interleaving in a fresh process with undefined-behavior checks."""
from pathlib import Path
import subprocess,tempfile,os
ROOT=Path(__file__).resolve().parents[1]
CASES=['camera_off_on','camera_gear','camera_rx_gap','camera_service_gap','rule_service_gap','sensor_gap','sensor_invalid','concurrent_toggles','disable_queued_rule']
with tempfile.TemporaryDirectory() as tmp:
    exe=Path(tmp)/'interleavings'
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all',str(ROOT/'tests/test_automation_interleavings.cpp'),'-o',str(exe)],check=True)
    for case in CASES:subprocess.run([str(exe),case],check=True)
print('All 9 production interleaving regressions PASS')
