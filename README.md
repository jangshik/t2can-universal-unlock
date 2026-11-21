> ⚠️ **Research / educational firmware only**
>
> This project interacts with a Tesla vehicle CAN bus. It is intended for controlled bench testing, code review, and research environments only.
>
> It sends signals directly to vehicle controllers; it is **not** a physical steering-wheel command. **Do not use it on public roads or in any situation where unsafe behavior could put people or property at risk.**
>
> You are responsible for your own testing, wiring, configuration, and compliance with local laws.

--- 

# T2CAN Universal Unlock v3.30.3

**Major Universal Release for Lilygo T2CAN**  
**Release date:** 20 11 2026  
**[Dashboard view](https://06066060606060.github.io/t2can-universal-unlock/)**  

**[仪表板模拟](https://06066060606060.github.io/t2can-universal-unlock/index-zh-cn.html)** 

### Discord
Join Discord server for more info:
https://discord.gg/euPbYG8Npc

---
**v3.30.3 **
- Restore TLSSC for Model YL
- Added Chinese Language variant
- **add_feats branch:** Body Comfort for HW4 Model Y Juniper — Easy Entry (window drop + restore, rain/snow block, mirror fold/camera/auto-unfold), 2nd-row seat-belt chime off per seat, steering-wheel macros, hazard blink, reverse hazards / rear light, D/R door-open hazards. Wi-Fi AP slow start and hidden SSID. All OFF by default — see [BODY_COMFORT.md](BODY_COMFORT.md).

**v3.30.2 **	
- Added Model S/X MCU2 HW3 profile (Compatibility with HW2.5) 
- Fix R79 injection after AP disengage. 
- Added experimental FSD Visualisation panel.(This will block AP Activation and cause autopark error!)
- Added Service Mode entry.
- Reorganized settings for a better overview. 
- Auto lane change enable for Standard AP 

**v3.28+ **	
- Continuous AP Experimental    
- Independent ISA chime Suppression 
- Increased OTA receive idle timeout from 5 to 30 seconds.
- Added detailed upload errors and improved retry handling.
- Prevented duplicate uploads and false success messages.
- The new timeout applies after installing and booting this version.  
- Clearer feature names and short descriptions  

**v3.26** 
 - Fixed initial setup recovery, added clear save-error messages, and prevented duplicate saves.
 - Added USB CAN logging and a persistent listen-only mode. (not for everyone)
 - Made ISA Suppression independent of Nag Killer.
 - Improved long diagnostic text layout on mobile screens.

**v3.23**  
 - Improved CAN reliability, including safer BUS-OFF recovery and expanded error diagnostics.  
 - Simplified CAN A reception with immediate single-frame processing.  
 - Removed completed or obsolete LAB tools, including Lane Graph, Parked 0x3FD Injection, and Blind Spot Injection Monitor.  
 - Fixed dashboard API recovery and mobile dropdown touch handling.  
 - Made Disable Driver Monitoring a standalone production feature, independent of Nag Killer.  
 - Promoted Visual Speed Control from LAB to Settings and removed 0x3FD from its name.  
 - Preserved existing AP, CAN health, signal freshness, routing, and transport safety checks.  
 - And a lot more check [Changelog](https://github.com/06066060606060/t2can-universal-unlock/blob/main/CHANGELOG.md)


## 🧩 add_feats — 2026-11-21 추가 기능 (Body Comfort + Wi-Fi Timing)

> 기준 차량: **HW4 Model Y Juniper · Body + Chassis** (Body CAN = CAN A / MCP2515). Model Y L(Party + VH)은 같은 VEH 프레임이 CAN B(VH)에 있어 동일하게 동작합니다. Party + Chassis, Model S는 미지원입니다. 다른 Body + Chassis 프로필(Highland, Legacy 3/Y)은 프레임 레이아웃이 검증되지 않아 "실험적"으로 표시됩니다.
>
> 모든 기능은 **기본 OFF**이며 각각 ON/OFF 할 수 있습니다. 대시보드: **Settings → Features → Body Comfort**, Wi-Fi: **Settings → Wireless → WI-FI TIMING**. 신호 정의는 Bayberry VEH DBC 기준이며, **호스트 시뮬레이션 테스트만 통과했고 실차 검증은 하지 않았습니다.** 오프라인 미리보기: [`docs/body-comfort-preview.html`](docs/body-comfort-preview.html).

### 공통 송신 규칙

- 모든 송신 프레임은 **방금 수신한 순정(stock) 프레임의 복사본**이며, 아래 표의 비트만 바꾸고 나머지 바이트는 그대로 둡니다. 송신은 순정 프레임을 받은 직후 같은 버스로 합니다.
- 송신 경로는 기존 CAN TX 복구 배리어(`canTxMcpSendTagged` / `canTxTwaiTransmitTagged`, TX trace 소스 `BODY_COMFORT`)를 그대로 거칩니다. USB 로거 listen-only, 프로필 설정 모드, CAN 유지보수 중에는 송신하지 않습니다.
- 비트 표기는 DBC와 같은 **Intel(little-endian) start|length**입니다. 예) `24|2` = 24번 비트부터 2비트.
- **원샷 명령**(창문, 미러): 순정 프레임 뒤에 수정 복사본을 **3프레임** 보냅니다. 2초 안에 1프레임 이상 보내면 성공으로, 하나도 못 보내면 타임아웃으로 처리합니다.
- **버튼 누름**(비상등, 카메라): 300 ms 동안 들어오는 순정 프레임마다 버튼 비트를 켠 복사본을 보냅니다. 이후 200 ms는 다음 누름을 받지 않습니다. 기존 stalkless 방향지시 주입과 같은 형태이고, 운전자가 실제로 그 버튼을 누르고 있으면 주입하지 않습니다.
- **상태 덮어쓰기**(후미등, 벨트): 조건이 유지되는 동안 해당 순정 프레임이 들어올 때마다 수정 복사본을 보냅니다. 순정 값이 이미 원하는 값이면 보내지 않습니다.

### 수정(송신)하는 비트

| ID | 메시지 / mux | 비트 (start\|len) | 신호 | 순정 → 수정 값 | 기능 |
|---|---|---|---|---|---|
| 0x294 | UI_vehicleControl3 | 42\|6 (signed, ×5 %) | UI_windowPercentageRequest | → `pct/5`. GOTO_PERCENT일 때만 쓰고, 그 외에는 0 | 1) 창문 복구 % |
| 0x294 | UI_vehicleControl3 | 48 / 49 / 50 / 51 | UI_windowRequestedFL / FR / RL / RR | → 대상 창문만 1, 나머지 0 | 1) 대상 창문 선택 |
| 0x294 | UI_vehicleControl3 | 52\|3 | UI_windowRequest | → 4 GOTO_OPEN / 3 GOTO_CLOSED / 2 GOTO_VENT / 1 GOTO_PERCENT | 1) 창문 열기 / 복구 |
| 0x273 | UI_vehicleControl | 24\|2 | UI_mirrorFoldRequest | → 1 RETRACT(접기) / 2 PRESENT(펴기) | 1) 미러, 매크로 |
| 0x273 | UI_vehicleControl | 23\|1 | UI_rearFogSwitch | 0 → 1 (R단 동안) | 6) 후진 후미등 (모드 FOG / BOTH) |
| 0x3B3 | UI_vehicleControl2 | 9\|3 | UI_lightSwitch | AUTO/OFF/… → 1 ON (R단 동안) | 6) 후진 후미등 (모드 LIGHTS / BOTH) |
| 0x3C2 | VCLEFT_switchStatus mux0 (bit0=0) | 3\|1 | VCLEFT_hazardButtonPressed | 0 → 1 (300 ms 누름) | 4) 5) 7) 비상등 토글 |
| 0x3C2 | VCLEFT_switchStatus mux1 (bit0=1) | 50\|2 | VCLEFT_swcCameraButtonState | 1 OFF → 2 ON (300 ms 누름) | 1) 접힘 중 카메라, 매크로 |
| 0x3A1 | VCFRONT_vehicleStatus mux0 (bit0=0) | 36\|2 / 38\|2 / 40\|2 | VCFRONT_2RowLeft / Center / RightUnbuckled | 1 OCCUPIED_AND_UNBUCKLED → 0 NONE (선택한 좌석만) | 2) 2열 벨트 알람 |
| 0x3A1 | VCFRONT_vehicleStatus | 52\|4, byte 7 | Counter, Checksum | 카운터는 그대로, 체크섬 재계산: `(0xA1 + 0x03 + byte0…byte6) & 0xFF` | 2) |

0x3A1은 **순정 프레임의 체크섬이 위 공식과 맞을 때만** 수정합니다. 맞지 않으면 아무것도 보내지 않고 `seatbeltBadChecksum` 카운터만 올립니다. 에어백/RCM이 쓰는 원본 벨트 신호(0x40A, 0x31A)는 건드리지 않습니다.

### 읽기만 하는 비트

| ID | 메시지 | 비트 | 신호 / 해석 | 용도 |
|---|---|---|---|---|
| 0x102 / 0x103 | VCLEFT / VCRIGHT_doorStatus | 14\|4, 18\|4 | front / rearLatchStatus: 1 OPENED, 3 CLOSING, 4 OPENING, 5 AJAR → 열림 / 2 CLOSED → 닫힘 / 그 외 → 알 수 없음 | 문 상태 (0x102 = FL·RL, 0x103 = FR·RR) |
| 0x102 / 0x103 | 〃 | 52\|3 | mirrorFoldState: 1 FOLDED, 2 UNFOLDED, 3 FOLDING, 4 UNFOLDING | 미러 상태 |
| 0x118 | DI_systemStatus | 21\|3 | DI_gear: 1 P, 2 R, 3 N, 4 D (1.5초 이내 값만 사용) | 기어 |
| 0x257 | DI_speed | 43\|13 | DI_vehicleSpeed = raw × 0.08 − 40 km/h (8191 = SNA) | 미러 펼침 속도 |
| 0x25C | APP_environment | 0, 1 | Rainy, Snowy (5초 이내 값만 사용) | 비/눈 차단 |
| 0x2C2 / 0x2C3 | VCLEFT / VCRIGHT_windowStatus mux0 | 1\|4, 33\|4 / 22\|9, 54\|9 (signed) | 창문 동작 상태(0 STOPPED) / 위치 raw | 정지 판정, 위치 학습 |
| 0x2C2 / 0x2C3 | 〃 mux1 | 49\|3, 52\|3 | windowPositionState: 1 CLOSED, 2 CRACKED, 3 VENT, 4 PARTIAL, 5 FULL_OPEN, 6 CLOSED_TRIM_CLEAR | 원래 위치 |
| 0x353 | UI_status | 41\|1 | UI_cameraActive | 카메라 표시 여부 |
| 0x3C2 | VCLEFT_switchStatus mux0 | 32–35 / 36–39 / 40–43 / 44–47 | 운전석 창문 스위치 팩 LF / LR / RF / RR | 운전자 조작 시 자동 복구 취소 |
| 0x3C2 | VCLEFT_switchStatus mux1 | 48\|2, 50\|2, 5\|2, 12\|2, 14\|2, 3\|2, 8\|2, 10\|2 | 음성인식 버튼(swcVoiceControlPress), 카메라 버튼(swcCameraButtonState), 왼쪽 휠 누름, 오른쪽 휠 누름, 왼쪽 휠 좌틸트, 왼쪽 휠 우틸트, 오른쪽 휠 좌틸트, 오른쪽 휠 우틸트 (2 = 눌림) | 매크로 입력 |
| 0x3F5 | VCFRONT_lighting | 4\|4 | hazardLightRequest: 0 NONE, 1 BUTTON, 2 이상 = 잠금 깜빡임 등 다른 소스 | 비상등 실제 상태 |
| 0x3F5 | VCFRONT_lighting | 0\|2 | indicatorLeftRequest (≠0 = 점등) | 점멸 횟수 카운트 |

### 기능별 상세

**Wi-Fi AP Slow Start**
- 부팅 후 웹/Wi-Fi 태스크만 N초(0–600, 기본 30) 기다린 뒤 AP를 켭니다.
- CAN, BLE, 다른 기능은 원래대로 바로 시작하고, 각자의 시작 대기시간도 그대로입니다.
- 프로필 설정 모드나 NVS 오류 모드에서는 복구를 위해 항상 바로 켭니다.

**Hide Wi-Fi SSID**
- AP가 켜지고 N초(0–3600, 기본 300) 지나면 `WiFi.softAP(ssid, pass, 1, ssid_hidden=1)`로 SSID를 숨깁니다.
- 재부팅은 일어나지 않습니다. AP만 재설정되며, 그 순간 접속해 있던 기기가 한 번 끊겼다가 저장된 네트워크로 다시 붙습니다.
- 끄면 바로 다시 보이고, SSID/비밀번호를 바꿔도 숨김 상태는 유지됩니다. 설정 모드에서는 숨기지 않습니다.
- NVS `wifiap`: `slowEn`, `startDly`, `hideEn`, `hideDly`. Reset Feature Settings를 해도 SSID처럼 보존됩니다.

**1) Easy Entry**
- 문마다 상태(문 마스크로 선택)를 따로 추적합니다. 동작 조건: 신선한 기어가 **P**, 문이 닫힘 → 열림으로 바뀜, 비/눈 차단에 걸리지 않음, 원래 창문 위치를 알고 있음.
- 조건이 맞으면 그 문의 창문 하나에 `GOTO_OPEN`을 보냅니다. 여러 문이 동시에 해당하면 같은 명령은 한 프레임에 묶어 보냅니다.
- **원래 위치**: 문이 닫혀 있고, 창문이 STOPPED이며, positionState가 **1.5초 동안 같았던** 값만 저장합니다. 손잡이를 당길 때 생기는 순정 short-drop 위치는 이렇게 걸러집니다.
- **복구**: 문이 CLOSED로 바뀌면 지연(0–5000 ms, 기본 800) 후 보냅니다.

  | 원래 위치 | 복구 명령 |
  |---|---|
  | CLOSED / CLOSED_TRIM_CLEAR | `GOTO_CLOSED` |
  | VENT | `GOTO_VENT` |
  | CRACKED / PARTIAL | `GOTO_PERCENT` (계산값이 5 % 미만이면 CLOSED) |
  | FULL_OPEN | 아무것도 안 함 (Easy Entry 자체를 실행하지 않음) |

  - `GOTO_PERCENT` 값은 창문이 CLOSED·정지일 때와 FULL_OPEN·정지일 때의 위치 raw를 학습해 `(현재 − 닫힘) / (열림 − 닫힘)`으로 구하고 5 % 단위로 반올림합니다.
  - 아직 학습 전이면 활짝 열어 두지 않도록 `GOTO_VENT`로 복구합니다. 실패하면 0.5초 뒤 한 번 더 시도합니다.
- **비/눈 차단**: 0x25C Rainy/Snowy 중 하나라도 1이면 창문을 내리지 않습니다(기본 ON). "날씨 정보 없음도 차단"을 켜면 0x25C가 5초 넘게 없을 때도 차단합니다.
- **취소**: 창문이 열려 있거나 복구를 기다리는 중에 운전석 창문 스위치 팩에서 그 창문을 조작하면 자동 복구를 취소합니다.
- **미러 접기(선택)**: Easy Entry가 동작할 때 미러가 접혀 있지 않으면 RETRACT를 보내고 "이 기능이 접음" 표시를 남깁니다.
  - 펴기 조건: 이 표시가 있을 때만, **D단 + 속도가 설정값(1–80, 기본 10 km/h) 초과 + 그 상태가 설정 시간(0–30, 기본 3초) 연속**이면 PRESENT를 보냅니다.
  - 운전자가 접은 미러는 절대 펴지 않습니다. 접은 지 8초 뒤 양쪽이 모두 UNFOLDED로 보이면 운전자가 직접 편 것으로 보고 표시를 지웁니다.
- **접힌 동안 카메라(선택, 기본 ON)**: 이 기능이 미러를 접은 상태에서 D/N단이고 `UI_cameraActive = 0`이면 카메라 버튼을 누릅니다(3초 간격, 최대 2회). 미러를 편 뒤에는 이 기능이 연 카메라만 다시 닫습니다.

**2) 2열 시트벨트 알람 제거 (좌석별)**
- 좌석 마스크: bit0 = 2열 좌, bit1 = 중, bit2 = 우.
- 0x3A1 mux0이 들어올 때마다, 선택한 좌석 값이 1이면 0으로 바꾸고 체크섬을 다시 계산해 보냅니다.

**3) 핸들 버튼 매크로**
- 버튼 8개 × 제스처 3개 = **24개 슬롯**입니다. 버튼은 **음성인식, 카메라**(대시보드 목록 맨 위), 왼쪽/오른쪽 휠 누름, 왼쪽/오른쪽 휠 좌·우 틸트입니다.
  - 음성인식·카메라 버튼은 값이 2일 때만 눌림으로 봅니다. 카메라 버튼이 INACTIVE/SOFT/HARD 형식이라면 가볍게 누른 SOFT(1)는 클릭으로 세지 않고, 끝까지 누른 HARD(2)만 셉니다.
  - **더블 / 트리플**: 누름 → 뗌을 한 번의 클릭으로 세고, 마지막으로 뗀 뒤 클릭 간격(200–800, 기본 400 ms)이 지나면 클릭 수로 판정합니다.
  - **롱**: 누른 채 500–2000 ms(기본 800)가 지나면 그 즉시 실행합니다. 이때 그 전까지 센 클릭은 무효가 됩니다.
  - 0x3C2 mux1이 600 ms 넘게 안 들어오면 판정을 초기화합니다. 처음 볼 때 이미 눌려 있던 버튼은 무시합니다.
- 동작 ID: 0 없음, 1 Easy Entry 토글, 2 미러 접기 옵션 토글, 3 벨트 알람 토글, 4 비상등 점멸 기능 토글, 5 후진 비상등 토글, 6 후진 후미등 토글, 7 문열림 비상등 토글, 8 비상등 점멸 실행, 9 미러 접기/펴기, 10 카메라.
- 토글한 결과는 1.5초 뒤 NVS에 저장됩니다.
- 각 버튼의 순정 기능은 막을 수 없어서 누를 때마다 같이 실행됩니다. 예를 들어 음성인식 버튼을 더블 클릭하면 음성인식도 열리고, 카메라 버튼을 누르면 카메라 화면도 바뀝니다. 매크로는 이 순정 동작에 더해 추가 동작을 실행합니다.

**4) 비상깜빡이 N회 점멸 (1–10, 기본 3)**
- 매크로(동작 8)나 대시보드 "Blink Now"로 실행합니다. 이미 비상등이 켜져 있으면(hazardLightRequest ≠ 0) 거부합니다.
- 실행하면 비상등 버튼을 눌러 켜고, hazardLightRequest = 1인 동안 indicatorLeftRequest가 꺼짐 → 켜짐으로 바뀐 횟수를 셉니다. N회가 되면 다시 눌러 끕니다.
- 안전장치로 `3초 + N × 1.2초`가 지나면 횟수와 상관없이 종료합니다.

**5) 후진 시 비상깜빡이** — R단이면 켜고, R단을 벗어나면 끕니다.

**6) 후진 시 후미등**
- R단 동안 모드에 따라 덮어씁니다: 0 = 후미 안개등(0x273 bit23), 1 = 라이트 ON(0x3B3 bit9–11), 2 = 둘 다(기본).
- 차에 따라 후미 안개등은 전조등이 켜져 있어야 점등될 수 있어서 기본값을 "둘 다"로 했습니다.

**7) D·R에서 문 열림 시 비상깜빡이** — D 또는 R에서 문이 하나라도 열리면 켜고, 모든 문이 닫힌 뒤 지연(0–10000, 기본 2000 ms)이 지나면 끕니다. 중간에 P로 바꿔도 문이 다 닫힐 때까지 유지합니다.

**비상등 소유권 규칙 (4·5·7 공통)**
- 실제 상태(0x3F5)와 요청이 다를 때만 버튼을 누르고, 1.2초 뒤 결과를 확인합니다. 요청 하나당 최대 3번까지 시도합니다.
- 이 기능이 켠 비상등만 끄고, 운전자가 켠 비상등은 건드리지 않습니다.
- 요청 중에 운전자가 비상등을 꺼버리면 그 요청이 끝날 때까지 다시 켜지 않습니다.
- 잠금 깜빡임 등 다른 소스(값 2 이상)가 비상등을 쓰는 중이면 끝날 때까지 기다립니다.

### API / 저장

- `GET /api/body/config` — 설정(`cfg`), 실시간 상태(`live`), 카운터(`stats`).
- `POST /api/body/update` — 키: `eeEnabled, eeDoorMask, eeWeatherBlock, eeWeatherStrict, eeRestoreDelayMs, eeMirrorFold, eeMirrorCamera, eeUnfoldKph, eeUnfoldHoldS, sbEnabled, sbSeatMask, mcEnabled, mcLongMs, mcGapMs, mc=<버튼>,<제스처>,<동작>, hzBlinkEnabled, hzBlinkCount, rvHazard, rvLight, rvLightMode, drHazard, drOffDelayMs`.
  - 범위를 벗어나면 400, 미지원 토폴로지는 409를 돌려줍니다.
- `POST /api/body/action?id=8|9|10` — 동작 즉시 실행.
- `GET|POST /api/wifi/timing` — 키: `slowStartEnabled, startDelayS, hideEnabled, hideDelayS`.
- 설정은 NVS `bodycmf/cfg`에 저장합니다(42바이트, 버전 2 + 체크섬). 버튼 6개였던 이전 36바이트(버전 1) 설정도 그대로 읽어 매크로를 유지합니다. 손상되면 기본값(전부 OFF)으로 돌아가고, Reset Feature Settings로도 지워집니다.

### 추가·변경된 파일

- `body_comfort_pure.h`(로직), `body_comfort_runtime.h`(버스 연결, 송신, 서비스 태스크 `bodyCmf`, NVS, HTTP).
- `can_runtime.h`(CAN A/B 수신 훅), `can_core.h`(TX trace 소스), `web_api.h`(라우트, Wi-Fi 타이밍, 설정 초기화), `.ino`(설정 로드, 태스크 시작).
- 대시보드: `dashboard_source.html`, `zh-CN/…`, `index_html.h`. 생성 도구는 `tools/add_body_comfort_dashboard.py`, `tools/build_body_comfort_preview.py`.
- 테스트: `tests/test_body_comfort_pure.cpp`, `tests/test_body_comfort_runtime_host.cpp`, `tests/test_wifi_ap_timing_host.py`, `tests/test_dashboard_body_comfort.js`.

### 실차 확인이 필요한 가정

- VC가 UI가 아닌 노드가 보낸 0x294 창문 요청도 수행하는지, `UI_windowPercentageRequest`가 "열림 %"인지.
- Juniper 오버헤드 비상등 버튼이 0x3C2 `hazardButtonPressed`로 보고되는지.
- Juniper 핸들에서 `swcCameraButtonState`가 카메라 화면을 여는지.
- 2열 벨트 알람이 0x3A1 요약 신호로 결정되는지.
- 후미 안개등이 전조등 OFF에서도 켜지는지.

---


## 📋3.0 Release Highlights  

- **One Universal firmware** supporting five Model 3/Y/YL vehicle profiles.
- **multiple operating modes:**  
  - Nag-killer + Eu-Unlock  
  - Advanced EAP + Eu-Unlock 
  - Nag-killer + Advanced EAP + Eu-Unlock (Model YL only)
- **Profile-based CAN topology and turn-signal routing** instead of separate firmware branches or manual transport selection.
- **Direct S3XY Button Bluetooth support** for up to **3 registered buttons**.
- **Runtime Bluetooth Master ON/OFF without MCU reboot**, while preserving saved devices, bonds, mappings, and Auto Connect settings.
- **Reworked Auto Blinker** with profile-aware routing and direction-specific ALC availability gating.
- **Independent R79 engine** with immediate reassertion and independent periodic refresh.
- **Summon Monitor** separated from R79 ownership.
- **Off-Highway ALC BETA** added as a normal vehicle feature.
- **TLSSC Highway Block** added: TLSSC can optionally turn **OFF on highways / controlled-access roads**.
- **CAN Research Capture** with Snapshot, RAW Transition, and Auto ALC Transition modes.
- **Dedicated LAB research area** for R79, ULC Blind Spot, ACC Follow Distance, and CAN capture.
- **New mobile dashboard architecture** with HOME, DEVICES, SETTINGS, and LAB.
- **Configurable Wi-Fi AP**, tiered reset behavior, and expanded CAN/BLE diagnostics.
- **Stronger CAN recovery safety** through the new TX recovery barrier / epoch model.
- **Added optional Pause NAG at 0 km/h** — default OFF   
- **Added topology-aware feature gating** for Advanced EAP, Body controls, NAG and EU Unlock  

**Advanced EAP**
- Automatically activates the turn signal.
- Delay can be configured from the dashboard.
- All lane-change safety features are maintained.
- The turn signal starts only when the vehicle requests a lane change.
- Lane changes can always be cancelled: On screen, using the open-door button or using s3xy button.

**EU Unlock**
- Bypass R79 EU restriction in AP.
- Expand Summon range to ±85 m.
- Expanded lateral acceleration limits.  
- Lane changes near forks are not disabled (EAP).  
- Instantaneous lane change on blinker (EAP).  
- No lane-change timeout once initiated (EAP).  
- Automatically takes forks and exits (EAP).  
- Toggle to activate TLSSC (EAP).  

**NAG-Killer**
- eliminate the "hands on the wheel" prompt while using Autopilot/FSD*

---

## 🚙 Supported profiles

| Configuration | NAG Killer | Advanced EAP | Pedal Map | EU Unlock |
|---|:---:|:---:|:---:|:---:|
| **Model YL · Party + Body** | ✅ | ✅ | ✅ | ✅ |
| **Standard Model 3/Y · Body + Chassis** | ❌✅ | ✅ | ✅ | ✅ | 
| **Standard Model 3/Y · Party + Chassis** | ✅ | ❌ | ❌ | ✅ |

> Model 3 Highland requires selecting the installed turn-control type during profile setup. Other supported profiles automatically use the Stalk configuration.

## 🔧 Hardware Target

This code is adapted for the **LilyGO / T-2Can**.

| Device | CAN Interface | CAN Bus | Function |
|---|---|---|---| 
| LilyGO / T-2Can | CAN A — MCP2515 | Body CAN | 
| LilyGO / T-2Can | CAN B — TWAI | Chassis CAN | 

## 🧩 Main features

### 🛡️ Vehicle-profile safety

A saved Vehicle Profile determines the CAN topology, turn-signal route, supported features, and profile-specific restrictions.

On first boot or after a major migration, firmware remains in a fail-closed setup state until a valid profile is saved:

- CAN transmission and injection are disabled.
- CAN recovery supervision is disabled.
- Bluetooth is not started.
- Wi-Fi and the Profile Setup page remain available.

> Migrating from pre-Universal v2.x firmware intentionally erases NVS before creating the Universal bootstrap configuration.

### 🚦 Advanced EAP and Auto Blinker

Advanced EAP can automatically request a turn signal when the vehicle requests a lane change, with configurable delay and retained cancellation options through the screen, door-open action, or S3XY button.

Auto Blinker is profile-aware and validates the requested direction before transmission:

- `DAS_autoLaneChangeState` must permit the requested left or right maneuver.
- The condition is checked when the request is armed and again when it fires.
- A lane becoming unavailable during the delay cancels the pending action.
- NOA must remain eligible through the configured stabilization interval.
- Door-open or S3XY cancellation can apply a shared, configurable pause; triggering cancel again releases it early.

> Direct S3XY left/right blinker actions remain independent from the Auto Blinker enable, stabilization, and pause states.

### 🔁 R79 and Summon

The production R79 path applies a fixed policy to accepted stock frames:

- `UI_applyEceR79` bit 19: forced to `0`.
- `UI_hardCoreSummon` bit 47: forced to `1`.
- SmartSummonOnly bit 18: selectable in **Settings → Summon** as `FORCE 0` or `STOCK`.

Each accepted stock mux1 frame is echoed with a bounded 2 ms queue wait. Each accepted mux2 frame schedules one +150 ms refresh. The LAB page now exposes a read-only R79 status card rather than transport/timing experiments.

A separate always-on Summon Monitor provides state observation, gate telemetry, ACA/SPR monitoring, park-state monitoring, and queue/transport telemetry.

### 💪 NAG Killer and AP Right Scroll

NAG Killer remains profile-gated. Mode H Rev.4 is available as a production profile in **Settings → NAG KILLER** with these defaults:

- Primary: 1.80–2.60 Nm
- Wait: 0.90–3.00 s
- Refractory: 0.50–1.50 s
- Opposite carrier: 0.10–0.60 Nm
- Tiered Hands-On thresholds: 0.40 / 2.00 Nm
- Visual Warning Rescue: ON after 0.5 s
- Hard pause while stopped

AP Right Scroll is available in Settings for Model YL and standard Model 3/Y profiles. A newly detected visual warning triggers an immediate UP/DOWN pair; if the warning stays active, it repeats every 1–5 seconds (default: 2 seconds). Physical right-scroll input always has priority.

### 🔘 S3XY Button support

- Register up to **three S3XY Buttons**.
- Enable or disable Bluetooth Master at runtime without rebooting.
- Saved devices, bonds, button mappings, and Auto Connect settings are retained.
- S3XY actions can control direct blinkers, lane-change cancellation, and CAN research capture.

## 🧪 Dashboard and LAB

The mobile dashboard has four persistent sections:

| Section | Purpose |
|---|---|
| **HOME** | Status and quick controls |
| **DEVICES** | S3XY / Bluetooth devices |
| **SETTINGS** | Supported production features and system configuration |
| **LAB** | Experimental tools and CAN research |

### 🔬 LAB tools

> ⚠️ LAB is intended for controlled research only.

- Read-only R79 status and bit telemetry.
- Experimental **Disable Driver Monitoring (NAG)** control using `0x3FD` mux1 bit 43, where supported.
- ULC Blind Spot research modes: Stock, Standard, Aggressive, and Mad Max.
- Auto Lane Change experiment.
- CAN capture: Snapshot, Raw Transition, and Auto ALC Transition.

> CAN Research Capture is RX-only: it observes and exports CAN traffic but does not replay captured frames.

## 📶 Wi-Fi and resets

### 📡 Wi-Fi access point

Default access point:

| Setting | Default |
|---|---|
| SSID | `T2CAN-****` |
| Password | `12345678` |
| Dashboard | [http://192.168.4.1](http://192.168.4.1) |

SSID and password can be changed from the dashboard. Applying a Wi-Fi change restarts only the access point; CAN, BLE, and the MCU continue running.

### ♻️ Reset options

| Action | What it clears | What it preserves |
|---|---|---|
| Reset Firmware Settings | Normal firmware settings | Vehicle profile, Wi-Fi, and S3XY/Bluetooth data |
| Reset Bluetooth Data | S3XY registry, mappings, bonds, discovery/cache | Bluetooth Master and Global Auto Connect settings |
| Factory Reset | All NVS data | Nothing; returns to Vehicle Profile Setup |

## 🔧 Hardware note

> ⚠️ Remove the two **120 Ω termination resistors** if they are not appropriate for your wiring. Leaving them in place can cause CAN signal errors.

<img width="407" height="180" alt="LILYGO-T-2CAN_9" src="https://github.com/user-attachments/assets/0d272b7e-bd82-408f-9ca1-239e6dab44d5" />

## ☕ Support the Project

<p align="left">
  <a href="https://tesla-mods.fkey.id/">
    <img height="30" alt="Donate with Crypto" src="https://img.shields.io/badge/Crypto-Donate-F7931A?style=for-the-badge&amp;logo=bitcoin&amp;logoColor=white">
  </a>
  <a href="https://www.paypal.com/donate/?hosted_button_id=Q2YMMMJYTAPJE">
    <img height="30" alt="Donate with PayPal" src="https://img.shields.io/badge/PayPal-Donate-00457C?style=for-the-badge&amp;logo=paypal&amp;logoColor=white">
  </a>
  <a href="https://www.buymeacoffee.com/xbmod">
    <img height="30" alt="Buy Me a Coffee" src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png">
  </a>
</p>

  **各位，开发这个很不容易，费了好大劲，请大家适当赞赏，让我有持续更新的动力**  
<p align="left">
<img width="250" alt="368" src="https://github.com/user-attachments/assets/003791f1-f670-44b9-8728-73e8b48a24a6" />

</p>
