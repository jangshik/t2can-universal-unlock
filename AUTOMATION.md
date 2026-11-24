# Automation (1단계) — automation 브랜치, 2026-11-24

T2CAN 안에서 도는 **"언제(WHEN) → 만약(IF) → 그러면(THEN)"** 규칙입니다. Body Comfort가 이미 읽고 있는 차량 신호로 트리거와 조건을 만들고, Body Comfort의 송신 경로로 동작을 실행합니다. 핸들 버튼 매크로와는 **별개**이며 매크로 동작은 바뀌지 않습니다.

- 대상: Body Comfort와 같습니다(HW4 Model Y Juniper Body + Chassis = CAN A, Model Y L = CAN B). 그 외 프로필에서는 동작하지 않습니다.
- 대시보드: **Settings → Automation**. 패널 스크립트는 패널을 열 때 `/automation.js`로 따로 받아옵니다(메인 대시보드 gzip 100 KB 한도 때문).
- 기본값: 자동화 전체 OFF, 규칙 없음. 새 규칙은 **테스트 모드(dry-run)** 로 시작합니다.
- 호스트 시뮬레이션 테스트만 통과했고 **실차 검증은 하지 않았습니다.** 오프라인 미리보기: [`docs/automation-preview.html`](docs/automation-preview.html).

## 규칙 구조

| 항목 | 내용 |
|---|---|
| 개수 | 규칙 16개, 규칙당 조건 4개, 동작 3개, 이름 31바이트(UTF-8, 한글 약 10자) |
| 트리거 | **변하면(state)**: `신호 비교 값`이 거짓→참이 되고 N초(0–250) 유지. **바뀌면(change)**: 신호가 `from`에서 `to`로 바뀌고 N초 유지(`from`/`to`는 "아무 값" 가능) |
| 조건 | 각 조건은 앞 조건과 **AND / OR**로 연결. **AND가 OR보다 먼저** 계산됩니다: `A AND B OR C AND D = (A AND B) OR (C AND D)`. 조건이 없으면 항상 통과 |
| 동작 | 최대 3개를 순서대로 실행. "대기 N초"로 사이에 시간을 둘 수 있음 |
| 재실행 대기(cooldown) | 실행 후 1–3600초 동안 같은 규칙의 트리거를 무시 |
| 재트리거 | 동작 실행 중 다시 트리거되면 무시(기본) 또는 처음부터 다시 실행 |
| 테스트 모드 | 아무것도 보내지 않고 "실행했을 동작"만 로그에 남김 |

트리거는 **변화(엣지)** 에서만 동작합니다. 이미 참인 상태로 보이기 시작한 값(부팅 직후, 차가 깨어난 직후, 3초 넘게 신호가 끊겼다 돌아온 경우)은 트리거로 보지 않습니다. 자동화를 켜는 순간에도 과거의 변화로 실행되지 않습니다. 신호가 3초 이내로 잠깐 끊긴 경우에는 마지막 값을 유지합니다.

조건에서 **신호가 없거나 오래된(stale) 값은 항상 거짓**입니다("≠" 비교도 거짓).

## 신호

값은 모두 정수입니다. ON/OFF 신호는 0/1.

| 신호 | 값 | 출처 (Body CAN) | 유효 시간 |
|---|---|---|---|
| 기어 | 1 P, 2 R, 3 N, 4 D | 0x118 `DI_gear` 21\|3 | 1.5 s |
| 속도 | km/h (−50–250) | 0x257 `DI_vehicleSpeed` 43\|13 | 1 s |
| 가속 페달 | % | 0x118 32\|8 × 0.4 | 1.5 s |
| 브레이크 | 0/1 | 0x118 19\|2 | 1.5 s |
| 문 4개, 아무 문 | 0 닫힘 / 1 열림 | 0x102/0x103 latch 14\|4, 18\|4 | 1.5 s |
| 앞문 외부 핸들 | 0/1 당김 | 0x102/0x103 10\|1 | 1.5 s |
| 잠금 | 0 해제 / 1 잠김 | 0x339 `VCSEC_simpleLockStatus` 54\|2 | 3 s |
| 창문 4개 | 0 닫힘, 1 벤트(CRACKED·VENT), 2 부분 열림, 3 완전 열림 | 0x2C2/0x2C3 mux1 `windowPositionState` | 2 s |
| 열린 창문 있음 | 0/1 | 위 4개 | |
| 비 / 눈 / 비 또는 눈 | 0/1 | 0x25C bit 0, 1 | 5 s |
| 미러 | 0 펼침, 1 접힘, 2 움직이는 중 | 0x102/0x103 52\|3 | 1.5 s |
| 카메라 화면 | 0/1 | 0x353 `UI_cameraActive` 41 | 3 s |
| 비상등 | 0/1 | 0x3F5 `hazardLightRequest` 4\|4 ≠ 0 | 1.5 s |
| 라이트 스위치 | 0 Auto, 1 On, 2 Parking, 3 Off | 0x3B3 `UI_lightSwitch` 9\|3 | 1.5 s |
| 2열 안전벨트 좌/중/우 | 0 착용·빈자리 / 1 미착용 | 0x3A1 mux0 36/38/40\|2 = 1 (체크섬 확인) | 1.5 s |
| 기능 상태 20개 | 0/1 | Body Comfort 설정 + 기존 기능 설정값 | 항상 |
| 플래그 1–8 | 0/1 | 사용자 스위치(NVS 저장) | 항상 |

기능 상태: Easy Entry, Easy Entry 미러 접기, 시트벨트 알람 제거, 후진 비상등, 후진 후미등, 문열림 비상등, 킥다운, 도어 핸들 매크로, 핸들 매크로, 비상등 점멸, Nag Killer, ISA 억제, 운전자 모니터링, Auto Blinker, TLSSC, 도어 버튼 취소, Visual Speed Control, Continuous AP, 배터리 예열, Listen-only.

## 동작

| 동작 | 파라미터 | 송신 / 경로 | 주행(D) |
|---|---|---|---|
| 비상등 점멸 | 1–60초 | 0x3C2 mux0 비상등 버튼 누름 (Body Comfort 점멸, "비상등 점멸" 기능이 꺼져 있어도 동작) | 허용 |
| 미러 | 접기 / 펴기 | 0x273 `UI_mirrorFoldRequest` 24\|2 RETRACT/PRESENT, 3프레임 | **접기 불가** |
| 창문 | 창문 선택(FL FR RL RR) + 벤트/닫기/열기 | 0x294 (Easy Entry와 같은 경로, "대체 열기 명령" 설정 따름) | 허용 |
| 카메라 화면 | — | 0x3C2 mux1 카메라 버튼 누름 | 허용 |
| 프렁크 열기 | — | 0x273 `UI_frunkRequest` (기존과 같이 P + 정지에서만) | **불가** |
| 후방 안개등 | 켜기 1–3600초 / 끄기 | 0x273 `UI_rearFogSwitch` 23 덮어쓰기(후진 후미등과 같은 방식) | **켜기 불가**, D로 바뀌면 즉시 꺼짐 |
| 기능 | 기능 + 끄기/켜기/전환 | Body Comfort 설정 저장 / 기존 기능은 대시보드와 같은 설정 함수 | 허용 |
| 페달 모드 | Chill / Standard / Sport / 차량 설정 | 0x334 기존 Pedal Response 경로(같은 AP·Summon 게이트) | 허용 |
| 플래그 | 플래그 + 끄기/켜기/전환 | 내부 | 허용 |
| 대기 | 1–600초 | — | — |

- **주행 차단 목록**(`AUTO_DRIVE_BLOCKLIST`, `automation_pure.h`): 미러 접기, 후방 안개등 켜기, 프렁크 열기. **기어가 D이거나 기어 정보가 없으면** 실행하지 않고 로그에 "blocked in D"를 남깁니다. 목록에 `{동작, 파라미터}` 한 줄을 추가하면 차단 대상이 늘어납니다(`AUTO_MATCH_ANY` = 모든 파라미터). 대시보드는 이 목록을 펌웨어에서 받아 표시합니다.
- 이 차단은 **자동화에만** 적용됩니다. 핸들 매크로의 미러 접기는 기존 그대로입니다.
- 후방 안개등: 후진 테스트에서 Juniper가 덮어쓴 값을 계속 받아들이지 않고 깜빡였습니다. 자동화의 안개등도 같은 방식이라 실차에서 확인이 필요합니다.
- 이미 그 상태이면("already set", "already folded") 아무것도 보내지 않습니다.

## 안전장치

- 전체 스위치(OFF면 모든 규칙 정지, 실행 중인 동작도 중단). 대시보드 "지금 실행"은 전체 스위치와 상관없이 동작하지만 테스트 모드와 주행 차단은 그대로 적용됩니다.
- 송신 제한: 1분에 동작 30개. 넘으면 "rate limited"로 건너뜁니다.
- 창문·미러·카메라 등이 이미 다른 명령을 처리 중이면 최대 3초 기다렸다가 실행하고, 그래도 안 되면 "busy (gave up)".
- Listen-only가 켜져 있으면 CAN을 보내는 동작은 "listen-only (not sent)"로 건너뜁니다(설정 변경·플래그는 실행).
- 기존 CAN TX 배리어, 프로필 설정 모드, CAN 유지보수 중 송신 금지가 그대로 적용됩니다.
- **차가 잠들어 Body CAN이 조용하면 신호가 없어 규칙도 실행되지 않습니다.**

## 템플릿

대시보드 "템플릿에서 추가"(모두 테스트 모드로 생성):

1. 비: 잠겨 있으면 창문 닫기 — `비 또는 눈 = 예` / `기어 = P AND 잠김` → 창문 4개 닫기
2. 잠글 때 창문이 열려 있으면 닫기 — `잠금 0→1` / `열린 창문 있음` → 창문 4개 닫기
3. 100 km/h 초과 10초: 킥다운 끄기
4. 세차 모드 켜기(플래그 1) — Easy Entry, Easy Entry 미러 접기, 문열림 비상등 끄기
5. 세차 모드 끄기(플래그 1) — 위 세 기능 다시 켜기

## 저장 / API

- NVS `bcauto/rules`: `[버전 1][개수]` + 규칙마다 `[슬롯][71바이트]` + 체크섬. 손상되면 규칙 없음으로 시작, 개별 규칙이 잘못되면 그 규칙만 버립니다. `bcauto/glob`: 전체 스위치 + 플래그. **Reset Feature Settings** 로 지워집니다.
- `GET /api/auto` — 전체 스위치, 플래그, 규칙(`r` = 35개 정수), 규칙별 실행 횟수·마지막 결과, 최근 로그 20개, 통계.
- `GET /api/auto/catalog` — 신호 목록(id, key, 종류, 범위), 기능 사용 가능 여부, 주행 차단 목록, 한도.
- `POST /api/auto/rule?idx=0-15&name=…&r=<35개>` — 저장(검증 실패 시 400 + 이유).
  순서: `enabled, dryRun, restart, cooldownS, trigType(1 state/2 change), trigSignal, trigOp(0 = 1 ≠ 2 > 3 ≥ 4 < 5 ≤), trigValue, trigFrom(-32768 = 아무 값), holdS`, 조건 4개 × `signal, op, or(1 = OR), value`, 동작 3개 × `type, a, b`.
- `POST /api/auto/delete?idx=`, `POST /api/auto/global?enabled=0|1`, `POST /api/auto/flag?idx=0-7&value=0|1`, `POST /api/auto/run?idx=` (지금 실행).

## 파일 / 테스트

- `automation_pure.h` (신호 목록, 검증, 트리거·조건 계산, 주행 차단 목록, NVS 형식), `automation_runtime.h` (신호 스냅샷, 동작 실행, NVS, HTTP).
- Body Comfort 변경: 자동화용 후방 안개등 덮어쓰기(`autoFogActive`), 점멸 시간 지정, 창문 명령 시작 함수 공통화, 0x3A1 2열 벨트 상태 저장, 서비스 틱 끝에서 `automationServiceTick()` 호출.
- 대시보드: `dashboard_automation.js` → `automation_ui_js.h`(`tools/build_dashboard.py`가 함께 생성), `tools/add_automation_dashboard.py`(설정 행·패널·로더).
- 테스트:
  - `tests/test_automation_pure.cpp` — 신호 목록, 검증, AND/OR, 트리거 엣지·유지·끊김, 주행 차단, NVS, 이름 정리
  - `tests/test_automation_runtime_host.cpp` — 실제 런타임을 가상 Juniper Body CAN으로: 비→창문 닫기, 조건 실패, 전체 스위치, 대기·cooldown·재트리거, 주행 차단(D·기어 없음), 안개등 시간/ D 해제, 테스트 모드, listen-only, busy 대기, 기능·페달·listen-only 동작, 송신 제한, 신호 스냅샷, HTTP·NVS
  - `tests/test_dashboard_automation.js` — Playwright: 소스/임베디드/중국어 대시보드, 390·320 px, 규칙 편집·저장 CSV, 플래그, 지금 실행, 삭제, 오류 표시, `automation_ui_js.h`가 소스와 일치하는지
  - 빌드: `g++ -std=c++17 -Wall -Wextra -Werror -I. -Itests tests/test_automation_runtime_host.cpp`
