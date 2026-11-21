# Body Comfort + Wi-Fi Timing (add_feats)

> ⚠️ 연구/교육용 펌웨어입니다. 아래 기능은 Bayberry(VEH/CH/PARTY) DBC 신호 정의를 기준으로 구현했고, **호스트(PC) 시뮬레이션 테스트만 통과**했습니다. 실차 CAN 로그·실차 동작 검증은 하지 않았습니다. 처음 사용할 때는 정차 상태에서 기능을 하나씩 켜서 확인하세요.

## 지원 차량 / 버스

| 프로필 | 토폴로지 | 사용 버스 | 상태 |
|---|---|---|---|
| Model Y Juniper (HW4) | Body + Chassis | CAN A (Body/VEH) | **기준 타깃** |
| Model Y L | Party + VH | CAN B (VH = VEH) | 지원 (같은 VEH 프레임) |
| Model 3 Highland / Legacy 3·Y | Body + Chassis | CAN A | 실험적 (레이아웃 미검증, 대시보드에 경고 표시) |
| Party + Chassis, Model S | — | — | 미지원 (API 409, UI 비활성) |

모든 기능은 **기본 OFF**이고 각각 ON/OFF 토글이 가능합니다. 대시보드: **Settings → Features → Body Comfort**, Wi-Fi 타이밍은 **Settings → Wireless → WI-FI TIMING**.

송신하는 모든 프레임은 **방금 수신한 순정(stock) 프레임의 복사본**에서 문서화된 필드만 바꾼 것이며, 기존 CAN TX 복구 배리어(`canTxMcpSendTagged` / `canTxTwaiTransmitTagged`, trace 소스 `BODY_COMFORT`)를 그대로 거칩니다. USB CAN 로거 listen-only 모드, 프로필 설정 모드에서는 아무것도 송신하지 않습니다.

## 기능

### Wi-Fi AP Slow Start
- 부팅 후 N초(0–600) 뒤에 AP를 시작합니다. CAN, BLE, 다른 기능들은 그대로 즉시 시작하고 각자의 대기시간을 유지합니다(웹/Wi-Fi 태스크만 대기).
- 프로필 설정 모드/NVS 오류 모드에서는 복구를 위해 **항상 즉시** 시작합니다.

### Hide Wi-Fi SSID
- AP 시작 후 N초(0–3600) 뒤 SSID를 숨깁니다(`ssid_hidden=1`로 AP 재공지 → 접속 중인 기기는 한 번 끊겼다가 저장된 네트워크로 재접속).
- 끄면 즉시 다시 보이게 됩니다. SSID/비밀번호 변경 후에도 숨김 상태가 유지됩니다. 설정 모드에서는 숨기지 않습니다.
- 설정은 `wifiap` NVS 네임스페이스(`slowEn`, `startDly`, `hideEn`, `hideDly`)에 저장되어 Reset Feature Settings 시에도 SSID처럼 유지됩니다.

### 1) Easy Entry
- **P단(신선한 0x118 DI_gear)** 에서 문이 닫힘→열림으로 바뀌면 그 문의 창문을 `UI_windowRequest=GOTO_OPEN`으로 끝까지 내립니다(문별 선택 가능).
- 문이 다시 **완전히 잠김(CLOSED)** 되면 설정한 지연(기본 800 ms) 후 원래 상태로 복구합니다.
  - 원래 CLOSED → `GOTO_CLOSED`, VENT → `GOTO_VENT`, 부분 개방 → `GOTO_PERCENT`(학습한 전/폐 위치로 계산, 5 % 단위), 원래 완전 개방 → 아무것도 안 함.
  - 부분 개방인데 아직 전/폐 위치를 학습하지 못했다면 활짝 열린 채로 두지 않고 VENT로 복구합니다.
  - "원래 위치"는 문이 닫혀 있고 창문이 1.5초 이상 정지해 있던 값만 사용하므로, 손잡이를 당길 때의 순정 short-drop 위치를 원래 위치로 착각하지 않습니다.
- **비/눈**(`APP_environment` 0x25C `Rainy`/`Snowy`)이면 창문을 내리지 않습니다. "날씨 정보 없음도 차단" 옵션을 켜면 신호가 없을 때도 차단합니다.
- 운전석 창문 스위치를 조작하면 해당 창문의 자동 복구를 취소합니다(사용자 우선).
- **사이드미러 접기(선택)**: Easy Entry가 동작할 때 `UI_mirrorFoldRequest=RETRACT`.
  - 이 기능으로 접힌 경우에만 **D단 + 설정 속도 초과(기본 10 km/h) + 설정 시간(기본 3 s) 지속** 시 `PRESENT`로 펼칩니다. 운전자가 직접 접은 미러는 절대 펼치지 않습니다. 운전자가 직접 펼치면 소유권을 해제합니다.
  - **접힌 동안 카메라 화면(선택)**: D/N단에서 `UI_cameraActive`가 꺼져 있으면 스티어링 카메라 버튼(0x3C2 mux1 `swcCameraButtonState`)을 한 번 누릅니다(최대 2회). 미러를 펼친 뒤 이 기능이 연 카메라는 다시 닫습니다.

### 2) 2열 시트벨트 알람 제거 (좌석별)
- `VCFRONT_vehicleStatus` 0x3A1 mux0의 `2RowLeft/Center/RightUnbuckled`가 `OCCUPIED_AND_UNBUCKLED(1)`이면 선택한 좌석만 `NONE(0)`으로 바꾼 복사본을 순정 프레임 직후 송신합니다. 카운터는 유지, 체크섬(Tesla additive)을 재계산합니다.
- 순정 체크섬이 이 알고리즘과 맞지 않으면 **송신하지 않고** 카운터(`seatbeltBadChecksum`)만 올립니다.
- 에어백/RCM이 쓰는 원본 벨트 신호(0x40A/0x31A)는 건드리지 않습니다.

### 3) 핸들 버튼 매크로
- 버튼: 좌/우 스크롤 휠 누름, 좌/우 휠 좌·우 틸트 (6개, 0x3C2 mux1).
- 제스처: 더블 클릭, 트리플 클릭, 롱 클릭(기본 800 ms) → 18개 슬롯 각각에 동작 지정.
- 동작: 각 기능 ON/OFF 토글(Easy Entry, 미러 접기 옵션, 시트벨트, 비상등 깜빡임, 후진 비상등, 후진 후미등, 문열림 비상등), 비상등 N회 점멸 실행, 미러 접기/펴기, 카메라 화면.
- 토글 결과는 1.5초 디바운스 후 NVS에 저장됩니다. 순정 단일 클릭 기능(음소거 등)은 그대로 동작합니다(차단 불가).

### 4) 비상깜빡이 N회(기본 3회) 점멸
- 매크로 또는 대시보드 "Blink Now"로 실행. 이미 비상등이 켜져 있으면 실행하지 않습니다.
- 비상등 버튼 누름(0x3C2 mux0 `hazardButtonPressed`, 기존 stalkless 방향지시 주입과 같은 300 ms 누름 형태)으로 켜고, `VCFRONT_lighting` 0x3F5의 방향지시 점등 횟수를 세어 N회 후 다시 눌러 끕니다.

### 5) 후진 시 비상깜빡이 ON / 7) D·R에서 문 열리면 비상깜빡이 ON
- 비상등 제어는 **소유권 컨트롤러**가 담당합니다: 실제 상태(0x3F5 `hazardLightRequest`)를 보고 다를 때만 누르고, 결과를 확인(1.2 s)하며, 최대 3회 재시도합니다.
  - 이 기능이 켠 비상등만 끕니다. 운전자가 켠 비상등은 건드리지 않습니다.
  - 요청 중에 운전자가 꺼버리면 그 요청 동안에는 다시 켜지 않습니다.
  - 잠금/해제 깜빡임 등 다른 소스가 사용 중이면 끝날 때까지 기다립니다.
- 5) R단이면 ON, R단을 벗어나면 OFF.
- 7) D 또는 R에서 문이 열리면 ON → 모든 문이 닫힌 뒤 설정 지연(기본 2 s) 후 OFF.

### 6) 후진 시 후미등 ON
- R단 동안 순정 프레임 직후 덮어쓰기: 모드 선택 — 후미 안개등(`UI_rearFogSwitch`, 0x273) / 라이트 ON(`UI_lightSwitch=ON`, 0x3B3) / 둘 다(기본).
- 후미 안개등은 차량 설정에 따라 전조등이 켜져 있어야 점등될 수 있어 기본값을 "둘 다"로 했습니다.

## 사용 CAN 신호 (Bayberry VEH DBC)

| ID | 메시지 | 사용 신호 | 용도 |
|---|---|---|---|
| 0x102 / 0x103 | VCLEFT/VCRIGHT_doorStatus | front/rearLatchStatus, mirrorFoldState | 문 상태, 미러 상태 |
| 0x118 | DI_systemStatus | DI_gear | P/R/N/D |
| 0x257 | DI_speed | DI_vehicleSpeed | 미러 펼침 속도 |
| 0x25C | APP_environment | Rainy, Snowy | 비/눈 차단 |
| 0x273 | UI_vehicleControl | UI_mirrorFoldRequest, UI_rearFogSwitch | **TX** 미러, 후미 안개등 |
| 0x294 | UI_vehicleControl3 | UI_windowRequest / Requested* / PercentageRequest | **TX** 창문 |
| 0x2C2 / 0x2C3 | VCLEFT/VCRIGHT_windowStatus | state, position, positionState | 창문 상태/학습 |
| 0x353 | UI_status | UI_cameraActive | 카메라 상태 |
| 0x3A1 | VCFRONT_vehicleStatus (mux0) | 2Row*Unbuckled | **TX** 2열 벨트 알람 |
| 0x3B3 | UI_vehicleControl2 | UI_lightSwitch | **TX** 라이트 ON |
| 0x3C2 | VCLEFT_switchStatus | mux0 hazardButtonPressed·창문 스위치 / mux1 휠 버튼·카메라 버튼 | 매크로 입력, **TX** 비상등/카메라 버튼 |
| 0x3F5 | VCFRONT_lighting | hazardLightRequest, indicatorLeftRequest | 비상등 상태 확인/점멸 카운트 |

## API

- `GET /api/body/config` — 설정 + 실시간 상태(`live`) + 카운터(`stats`).
- `POST /api/body/update?<key>=<value>...` — 부분 업데이트. 키: `eeEnabled, eeDoorMask(0-15), eeWeatherBlock, eeWeatherStrict, eeRestoreDelayMs(0-5000), eeMirrorFold, eeMirrorCamera, eeUnfoldKph(1-80), eeUnfoldHoldS(0-30), sbEnabled, sbSeatMask(0-7), mcEnabled, mcLongMs(500-2000), mcGapMs(200-800), mc=<button>,<gesture>,<action>, hzBlinkEnabled, hzBlinkCount(1-10), rvHazard, rvLight, rvLightMode(0 fog/1 lights/2 both), drHazard, drOffDelayMs(0-10000)`. 범위 밖 값은 400, 미지원 토폴로지는 409.
- `POST /api/body/action?id=8|9|10` — 비상등 점멸 / 미러 접기·펴기 / 카메라.
- `GET|POST /api/wifi/timing` — `slowStartEnabled, startDelayS(0-600), hideEnabled, hideDelayS(0-3600)`.
- NVS: `bodycmf/cfg`(버전+체크섬 36바이트 blob, 손상 시 기본값). Reset Feature Settings가 `bodycmf`를 지웁니다.

## 검증 범위 / 미검증 항목

- 통과: `tests/test_body_comfort_pure.cpp`(디코더, 제스처, 비상등 소유권, 창문 복구 계획, 시트벨트 체크섬, 설정 blob), `tests/test_body_comfort_runtime_host.cpp`(실제 런타임 헤더를 스텁으로 컴파일 후 가상 Juniper Body CAN 트래픽으로 10개 시나리오), `tests/test_wifi_ap_timing_host.py`(실제 web_api.h Wi-Fi 섹션 컴파일), `tests/test_dashboard_body_comfort.js`(Playwright: 소스/임베디드/중국어 대시보드).
- ESP32-S3 컴파일/링크 통과 (2026-11-21, Arduino CLI 1.5.1, ESP32 core 3.3.12, autowp-mcp2515 1.3.1, `Compile-T2CAN-Variants.ps1`과 같은 보드 옵션):
  - EN: 프로그램 1,597,113 / 3,145,728 B (50%), 정적 RAM 94,108 / 327,680 B (28%).
  - zh-CN: 프로그램 1,600,729 B (50%), 정적 RAM 94,108 B.
  - 기준 커밋 `7463e08` 대비: 플래시 +22.7 KB, RAM +648 B. `--warnings all`에서 새 코드 경고 0건.
  - 실제 보드에 플래시/부팅해 보지는 않았습니다.
- 실차 미검증 가정(실차 로그로 확인 필요):
  - `UI_vehicleControl3`(0x294) 창문 요청을 VC가 UI 이외 노드의 프레임으로도 수행하는지, `UI_windowPercentageRequest`가 "열림 %"인지.
  - Juniper에서 비상등 버튼이 `VCLEFT_switchStatus.hazardButtonPressed`로 보고되는지(오버헤드 PRND/비상등 모듈 경유).
  - Juniper 핸들에서 `swcCameraButtonState`가 카메라 화면을 여는지.
  - 2열 벨트 알람이 0x3A1 요약 신호로 결정되는지.
  - 후미 안개등이 전조등 OFF 상태에서도 켜지는지.
