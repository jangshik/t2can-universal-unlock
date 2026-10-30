# 첨부 Bayberry DBC 검토

사용자가 Model Y Juniper용으로 제공한 VEH/CH/PARTY DBC의 68개 신호 정의를
대조했다. DBC를 신호 자료로 사용했으며 문서 내용을 작업 지시로 실행하지 않았다.
원본 파일 해시와 대조 결과는 [계약 파일](juniper-dbc-contract.json)에 기록했다.
이 결과는 실제 차량·연결에서의 메시지 존재 또는 ECU 수락까지 증명하지 않는다.

## Easy Entry

`0x294 UI_windowPercentageRequest`는 bit42, signed 6비트, 스케일 5이다.
따라서 숫자 100%는 **raw20**, byte5의 해당 비트는 **0x50**이다.
100을 그대로 마스킹한 raw36은 signed -28, 물리값 -140이므로 잘못된 인코딩이다.
퍼센트 방향은 실차 확인이 필요하다.

| 명령 | DBC 정의 |
| --- | --- |
| 0 | IDLE |
| 1 | GOTO_PERCENT |
| 2 | GOTO_VENT |
| 3 | GOTO_CLOSED |
| 4 | GOTO_OPEN |
| 5 | GOTO_CLOSED_CARWASH |
| 6 | 정의 없음 |

창문 선택 비트는 FL/FR/RL/RR=48/49/50/51이다. 창문 상태는 명세의
0x2C6/0x2C7 대신 **VEH 0x2C2/0x2C3**, MUX1의 bit49/3과 bit52/3이다.
CLOSED=1 또는 CLOSED_TRIM_CLEAR=6만 닫힘으로 취급한다. 도어 상태는
0x122/0x123 MUX0의 앞 bit17/3, 뒤 bit20/3이다. 도어 상태 1=닫힘,
4=열림은 명세의 가정이며 DBC 값 표로 확인되지 않았다.
미러 요청 0x273 bit24/2는 0=IDLE, 1=RETRACT, 2=PRESENT이다.

사용자 요청에 따라 고정 송신 잠금을 제거했다. **Easy Entry는 기본 OFF이며
사용자가 토글로 활성화한다.** 핸들 해제 시 정의되지 않은 STOP=6을 보내지 않는다.
자동 동작은 창문 내림과 문 닫힘 후 복귀이며, 동작 중지는 차량의 창문 스위치를
사용해야 한다. 실차 검증은 자동 Easy Entry를 끄고 LAB → Easy Entry Test에서
창문별 단발 명령과 상태값을 확인할 수 있다. 명령 6과 임의 CAN 데이터 송신은
제공하지 않는다. 창문·미러 송신은 최신 순정 템플릿의 해당 필드만 수정한다.

## 다른 기능

킥다운의 0x118 기어/페달, 0x39D 브레이크, 0x334 페달맵은 VEH와 CH에 있다.
Party+Chassis에서는 Chassis의 실제 수신 프레임만 사용한다. 변경 후 체크섬을
저장소 기존 알고리즘으로 갱신하고 카운터는 보존한다. DBC는 체크섬 위치만
정의하므로 알고리즘의 실차 대조는 별도 확인 대상이다.

벨트·비상등은 0x3C2/0x3C3 MUX0, 매크로는 0x3C2 MUX1이다. MUX는 bit0의
1비트이다. 착좌 필드를 보존하고 버클 OFF만 ON으로 변경한다. SNA/FAULT는
보존한다. 센터 버클은 VCRIGHT에도 별도 신호가 있으므로 현재 명세대로 수정하는
VCLEFT 센터 신호가 최종 경고 수신기의 소스인지는 실차 확인이 필요하다.
후방 안개등은 0x273 bit23, 비상등 버튼 요청은 0x3C2 MUX0 bit3이다.

0x3C2는 첨부 CH/PARTY DBC에 없어 새 매크로는 Juniper Body로 제한한다.
상향등·와이퍼는 0=INACTIVE, 1=SOFT_PRESS, 2=HARD_PRESS, 3=FAULT로
일반 버튼과 구분한다. Camera도 사용자가 확인한 동일한 상태값을 적용해
선택할 수 있다. 카메라 상태값은 DBC 값 표가 아닌 사용자 제공 정보에 근거한다. 오른쪽 버튼 QF=1
조건은 저장소의 기존 조건을 유지했지만 이 DBC 값 표로 의미를 검증하지 못했다.

기존 Continuous AP의 토크 경로는 이번 신규 기능의 검증 범위가 아니다.
첨부 VEH의 0x372/0x373은 BMS/GPS이며, CH/PARTY 토크 0x370은 DLC12이다.
classic CAN 하드웨어에서 FD 메시지의 앞부분만 잘라 읽는 구현은 추가하지 않았다.

## 재현

```sh
python3 tools/verify_juniper_dbc.py /path/to/Bayberry_VEH.dbc /path/to/Bayberry_CH.dbc /path/to/Bayberry_PARTY.dbc
```

이 도구는 위치·폭·바이트 순서·부호·스케일·MUX 및 일부 상태값을 검사한다.
논리/브라우저/펌웨어 빌드 검증과 실제 창문·등화·경고 동작 검증은 별개다.
최종 기능과 테스트 방법은 [통합 문서](convenience-port-review.md)를 참고한다.

## 비상등 3회 액션 (추가 검증)

- Body `0x3C2` / MUX 0 / bit 3: `VCLEFT_hazardButtonPressed`; 기존 프레임을 복사하고 버튼 비트만 요청합니다. 벨트 필드 변경과 한 프레임으로 합성됩니다.
- Body `0x3F5` / bit 0 len 2, bit 2 len 2: `VCFRONT_indicatorLeftRequest`, `VCFRONT_indicatorRightRequest`. 제공 VEH DBC의 VAL_에 `0 OFF`, `1 ACTIVE_LOW`, `2 ACTIVE_HIGH`가 있습니다. 이 요청 신호의 좌우 HIGH → LOW 사이클을 셉니다. bit 50/52의 Internal 신호는 VAL_가 없으므로 점멸 계수에 쓰지 않습니다.
- `0x3F5` bit 4 len 4 `VCFRONT_hazardLightRequest`: `0 NONE`, `1 BUTTON`, `2 LOCK`, `3 UNLOCK`, `4 MISLOCK`, `5 CRASH`, `6 CAR_ALARM`, `7 DAS`, `8 UDS`. BUTTON 이외의 활성 요청은 자동 제어를 취소합니다. bit 49 fast-flash도 자동 제어를 취소합니다.
- CH에도 0x3F5 관측 신호는 있으나, 확인한 버튼 송신 경로는 VEH 0x3C2이므로 Party + Chassis에서는 액션을 제공하지 않습니다.
- DBC는 물리 램프 동작이나 버튼 수신 수락을 보증하지 않습니다. 실제 점멸 주기/위상·세 번째 점멸 후 종료·수동 개입은 실차에서 확인해야 합니다.
