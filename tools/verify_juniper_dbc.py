#!/usr/bin/env python3
"""Check the supplied Bayberry DBC contracts without executing DBC contents.

Usage: python3 tools/verify_juniper_dbc.py /path/to/Bayberry_*.dbc
Only Python's standard library is required. This verifies signal definitions,
not vehicle identity, ECU acceptance, checksum algorithms or physical behavior.
"""
import hashlib
import json
from pathlib import Path
import re
import sys


def read_dbc(path):
    messages, values = {}, {}
    current = None
    for line in path.read_text(errors="replace").splitlines():
        match = re.match(r"BO_ (\d+) (\w+): (\d+) ", line)
        if match:
            current = {"name": match[2], "dlc": int(match[3]), "signals": {}}
            messages[int(match[1])] = current
        match = re.match(
            r"\s+SG_ (\w+)\s*(M|m\d+)?\s*:\s*(\d+)\|(\d+)@([01])([+-])"
            r"\s*\(([^,]+),([^\)]+)\)", line)
        if match and current is not None:
            current["signals"][match[1]] = [int(match[3]), int(match[4]),
                int(match[5]), match[6], float(match[7]), float(match[8]), match[2]]
        match = re.match(r"VAL_ (\d+) (\w+) (.*);", line)
        if match:
            values[(int(match[1]), match[2])] = {
                int(k): v for k, v in re.findall(r'(\d+) "([^"]*)"', match[3])}
    return messages, values


def verify(paths):
    sources = {p.stem.removeprefix("Bayberry_"): (p, *read_dbc(p)) for p in paths}
    assert set(sources) == {"VEH", "CH", "PARTY"}, "Supply all three Bayberry DBC files"
    checks = []

    def field(bus, ident, name, start, size, scale=1, offset=0, sign="+", mux=None, order=1):
        actual = sources[bus][1][ident]["signals"][name]
        expected = [start, size, order, sign, scale, offset, mux]
        assert actual == expected, (bus, hex(ident), name, actual, expected)
        checks.append({"bus": bus, "id": hex(ident), "signal": name, "definition": actual})

    for bus in sources:
        field(bus, 0x118, "DI_gear", 21, 3)
        field(bus, 0x118, "DI_accelPedalPos", 32, 8, .4)
        field(bus, 0x257, "DI_uiSpeed", 24, 9)
        field(bus, 0x257, "DI_uiSpeedUnits", 33, 1)
        field(bus, 0x129, "SCCM_steeringAngle", 16, 14, .1, -819.2)
        field(bus, 0x129, "SCCM_steeringAngleValidity", 30, 2)
        field(bus, 0x129, "SCCM_steeringAngleSensorStatus", 14, 2)
        field(bus, 0x39D, "IBST_driverBrakeApply", 16, 2)
        assert sources[bus][1][0x39D]["dlc"] == 8
    for bus in ("VEH", "CH"):
        field(bus, 0x334, "UI_pedalMap", 5, 2)
        field(bus, 0x334, "UI_powertrainControlCounter", 52, 4)
        field(bus, 0x334, "UI_powertrainControlChecksum", 56, 8)
        field(bus, 0x3F5, "VCFRONT_indicatorLeftInternal", 50, 2)
        field(bus, 0x3F5, "VCFRONT_indicatorRightInternal", 52, 2)
        for name, start in (("VCFRONT_indicatorLeftRequest", 0), ("VCFRONT_indicatorRightRequest", 2)):
            field(bus, 0x3F5, name, start, 2)
            assert sources[bus][2][(0x3F5, name)] == {0: "OFF", 1: "ACTIVE_LOW", 2: "ACTIVE_HIGH"}
        field(bus, 0x3F5, "VCFRONT_hazardLightRequest", 4, 4)
        field(bus, 0x3F5, "VC_fastFlashHazardsActive", 49, 1)
        assert sources[bus][1][0x3F5]["dlc"] == 8
        assert sources[bus][2][(0x3F5, "VCFRONT_hazardLightRequest")] == {
            0: "NONE", 1: "BUTTON", 2: "LOCK", 3: "UNLOCK", 4: "MISLOCK",
            5: "CRASH", 6: "CAR_ALARM", 7: "DAS", 8: "UDS"}

    field("VEH", 0x294, "UI_windowPercentageRequest", 42, 6, 5, sign="-")
    field("VEH", 0x294, "UI_windowRequest", 52, 3)
    for index, corner in enumerate(("FL", "FR", "RL", "RR")):
        field("VEH", 0x294, "UI_windowRequested" + corner, 48 + index, 1)
    for ident, prefix, corners in ((0x2C2, "VCLEFT", ("LF", "LR")), (0x2C3, "VCRIGHT", ("RF", "RR"))):
        for corner, bit in zip(corners, (49, 52)):
            field("VEH", ident, prefix + "_windowPositionState" + corner, bit, 3, mux="m1")
    for ident, prefix in ((0x122, "VCLEFT"), (0x123, "VCRIGHT")):
        field("VEH", ident, prefix + "_frontDoorState", 17, 3, mux="m0")
        field("VEH", ident, prefix + "_rearDoorState", 20, 3, mux="m0")
    field("VEH", 0x273, "UI_mirrorFoldRequest", 24, 2)
    field("VEH", 0x273, "UI_rearFogSwitch", 23, 1)
    field("VEH", 0x61D, "UI_weatherSvcRaining", 56, 1)
    field("VEH", 0x61D, "UI_weatherSvcSnowing", 57, 1)
    field("VEH", 0x3C2, "VCLEFT_switchStatusIndex", 0, 1, mux="M")
    field("VEH", 0x3C2, "VCLEFT_hazardButtonPressed", 3, 1, mux="m0")
    for name, bit in (("rearLeftBuckleSwitch", 52), ("rearCenterBuckleSwitch", 62),
                      ("rearLeftOccupancySwitch", 56), ("rearCenterOccupancySwitch", 54)):
        field("VEH", 0x3C2, "VCLEFT_" + name, bit, 2, mux="m0")
    for name, bit in (("rearRightBuckleSwitch", 46), ("rearRightOccupancySwitch", 54)):
        field("VEH", 0x3C3, "VCRIGHT_" + name, bit, 2, mux="m0")
    for name, bit in (("swcLeftPressed", 5), ("swcRightPressed", 12),
                      ("swcLeftTiltRight", 3), ("swcRightTiltLeft", 8),
                      ("swcRightTiltRight", 10), ("swcWiperButtonState", 22),
                      ("swcHighBeamButtonState", 46), ("swcVoiceControlPress", 48)):
        field("VEH", 0x3C2, "VCLEFT_" + name, bit, 2, mux="m1")
    for name in ("VCLEFT_swcWiperButtonState", "VCLEFT_swcHighBeamButtonState"):
        assert sources["VEH"][2][(0x3C2, name)] == {0: "INACTIVE", 1: "SOFT_PRESS", 2: "HARD_PRESS", 3: "FAULT"}
    window_values = sources["VEH"][2][(0x294, "UI_windowRequest")]
    assert window_values[1] == "GOTO_PERCENT" and window_values[4] == "GOTO_OPEN"
    assert 6 not in window_values, "Re-review STOP behavior against this new DBC"
    for bus in ("CH", "PARTY"):
        assert 0x3C2 not in sources[bus][1]
        assert sources[bus][1][0x370]["dlc"] == 12
    assert sources["VEH"][1][0x372]["name"] == "BMS_log1"
    assert sources["VEH"][1][0x373]["name"] == "UI_gpsVehicleSpeed"
    return {"sources": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
            "verified_fields": checks, "window_request_values": window_values,
            "limits": ["DBC alone does not prove vehicle compatibility or ECU acceptance",
                       "Window STOP=6 is undefined; no stop-on-release command is sent",
                       "CH/PARTY torque is CAN FD DLC12, not classic CAN"]}


if __name__ == "__main__":
    print(json.dumps(verify([Path(p) for p in sys.argv[1:]]), indent=2))
