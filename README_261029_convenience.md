# 261029 Convenience — 추가 기능 및 펌웨어 안내

Model Y Juniper 편의 기능을 추가한 사전 릴리스입니다.
기준은 기존 `pre-release`의 `4298151`이며, 작업 브랜치는 `261029_convenience`,
최초 릴리스 태그는 `v261029_convenience`입니다. 펌웨어의 기존 버전 표기는 유지합니다.
카메라 매크로 활성화는 해당 태그 이후 브랜치 업데이트에 포함됩니다.

## 추가 기능

설정 진입: **Settings → Convenience & Macros**.
새 편의 기능과 매크로의 초기값은 OFF이며, 활성화 여부와 매개변수는 저장됩니다.
CAN Transmission은 별도의 기존 송신 모드 설정을 사용합니다.

| 기능 | 동작과 설정 | Body + Chassis | Party + Chassis |
| --- | --- | --- | --- |
| CAN Transmission | OFF로 두면 두 CAN 컨트롤러를 Listen Only로 전환 | 지원 | 지원 |
| Kickdown | Chill 상태에서 설정한 페달 임계값을 넘으면 Sport 요청. 해제는 페달·브레이크·둘 중 하나 선택 | Body 경로 | Chassis 경로 |
| Easy Entry Window | P에서 닫힌 창문·도어 상태·건조 날씨·새 핸들 당김을 확인해 창문 내림 요청, 문 닫힘 전이 후 복귀 요청 | 지원, 기본 OFF | 미지원 |
| Mirror Sync | 앞문 Easy Entry 동작에 맞춰 미러 펼침 요청 | 지원 | 미지원 |
| Rear Belt Left / Center / Right | 후열 좌석별 착좌 상태에 따라 버클 OFF를 ON으로 변경. SNA/FAULT와 착좌 필드는 보존 | 지원 | 미지원 |
| Reverse Hazard | 후진 진입·이탈 시 비상등 버튼 요청. 송신 성공 여부와 자동 제어 소유권 추적 | 지원 | 미지원 |
| Reverse Rear Fog | 후진 중 후방 안개등 요청 | 지원 | 미지원 |
| Custom Macros | 핸들 버튼 더블·트리플·롱 프레스에 기능 배정. 3개 슬롯과 시간 설정 | 지원 | 미지원 |

**Continuous AP - flipper는 제거했습니다. 기존 Continuous AP는 유지합니다.**

### 킥다운

- 기본 진입 임계값 90%, 해제 임계값 10%. 진입은 임계값 **초과**, 페달 해제는 **미만**입니다.
- 해제 조건은 페달, 브레이크 또는 둘 중 하나이며 기본값은 둘 중 하나입니다.
- 브레이크가 적용되면 어느 해제 모드에서도 주입을 중단합니다.
- D가 아니거나 입력이 오래됐거나 유효하지 않으면 동작하지 않습니다.
- 기존 수동 Pedal Response와 AP Accel / Regen 제어가 우선합니다.

### 핸들 매크로와 S3XY BLE

3개 슬롯마다 버튼·제스처·기능을 선택합니다. 디바운스 기본 30 ms,
클릭 간격 350 ms, 롱 프레스 800 ms이며 UI에서 변경할 수 있습니다.
더블클릭은 트리플클릭 판별 간격이 지난 뒤 실행하고, 롱 프레스는 누르는 동안
한 번만 실행합니다. 차량의 순정 버튼 기능도 함께 동작합니다.

선택 가능한 버튼은 좌/우 휠 누름, 좌 휠 오른쪽 기울임, 우 휠 좌/우 기울임,
상향등, 와이퍼, 음성, 카메라입니다. 카메라는 사용자가 확인한 상태값
`0=inactive, 1=soft_press, 2=hard_press, 3=fault`를 사용합니다.
Soft→Hard 전환은 한 번의 누름으로 처리하고 Fault는 진행 중 제스처를 취소합니다.

S3XY BLE와 매크로는 공통 기능 목록을 사용합니다. 신규 편의 기능뿐 아니라 기존
대시보드의 전역 기능 토글도 배정할 수 있으며 차량 프로필·연결별 지원 조건을
적용합니다. 기존 BLE 액션 ID 0–13은 유지하고 새 ID 14–53을 추가했습니다.
Bluetooth 자체를 끄면 BLE 버튼이 연결 해제되므로 대시보드에서 다시 켜야 합니다.

## Easy Entry 사용 및 LAB 테스트

Easy Entry는 **토글을 켜면 자동 송신이 허용**됩니다. 기본값은 OFF입니다.
창문 요청값 100%는 DBC의 signed 6비트·5% 스케일에 따라 **raw20**으로 인코딩합니다.
퍼센트의 물리적 방향 및 도어 코드 `1=닫힘 / 4=열림`은 실차 검증 대상입니다.

DBC에 STOP 명령 6이 정의되어 있지 않아 이를 전송하지 않습니다.
**핸들을 놓아도 창문 내림이 정지하지 않습니다.** 중지는 차량 창문 스위치를 사용하세요.
창문·미러 요청은 최신 원본 CAN 프레임의 해당 필드만 변경합니다.

실차 값 검증 절차:

1. Juniper **Body + Chassis** 프로필을 선택하고 차량을 P에 둡니다.
2. **Easy Entry Window를 OFF**로 두고 CAN Transmission과 LAB Menu를 켭니다.
3. **LAB → Easy Entry Test**를 엽니다.
4. 창문 하나를 선택하고 Percentage / Vent / Close / Open fully 명령을 선택합니다.
5. Percentage 명령은 0–100%, 5% 단위로 지정한 뒤 **Send once**를 누릅니다.
6. 창문 반응과 화면의 도어·창문 raw 값, 수신 경과시간, 성공 송신 횟수,
   마지막 `0x294` 8바이트를 함께 확인합니다.

최신 P 기어(500 ms 이내), idle 상태의 원본 창문 프레임(250 ms 이내), 해당 프로필,
송신 활성화를 요구합니다. 송신 직전에도 조건을 다시 확인합니다.
실패한 명령은 자동 재시도하지 않고, 테스트 명령을 저장하거나 부팅 후 재생하지 않습니다.
창문이 움직이므로 개구부를 비운 상태에서 시험하세요.

## 릴리스 파일 선택

대상은 **ESP32-S3 / Flash 16 MB / OPI PSRAM** 구성입니다.

| 파일 | 용도 |
| --- | --- |
| `T2CAN-261029_convenience-OTA.bin` | 기존 장치의 웹 OTA 화면에 업로드할 앱 이미지 |
| `T2CAN-261029_convenience-USB-merged.bin` | USB로 주소 `0x0`부터 기록할 부트로더·파티션·앱 통합 이미지 |
| `SHA256SUMS.txt` | 첨부 파일 무결성 확인 |
| `build-info.json` | 소스 커밋, 보드 옵션, 도구 버전, 파일 크기·해시 |
| `README_261029_convenience.md` | 이 사용 안내 |

OTA에는 **OTA.bin**을 사용합니다. merged 이미지는 16 MB 전체 플래시 범위를
기록하므로 NVS 설정과 저장 데이터가 초기화될 수 있습니다. USB 설치 전에 필요한
설정과 데이터를 보관하세요. 기존 파티션이 `app3M_fat9M_16MB`와 다르면
OTA만으로 파티션 구성을 바꿀 수 없습니다.

예시(포트 이름은 실제 장치에 맞게 변경):

```sh
python -m esptool --chip esp32s3 --port /dev/ttyACM0 write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 T2CAN-261029_convenience-USB-merged.bin
sha256sum -c SHA256SUMS.txt
```

## 빌드와 검증

- ESP32 Arduino core **3.3.11**, autowp-mcp2515 **1.3.1**.
- ESP32-S3, 16 MB Flash, OPI PSRAM, `app3M_fat9M_16MB`, hardware CDC,
  CDC on boot, loop/events core 1.
- 전체 펌웨어 컴파일·링크 성공. 기존 기능 검증에서 호스트 테스트 25개와
  브라우저 테스트 3개 통과(소스 및 내장 대시보드 모두 검사).
- 첨부 Bayberry VEH/CH/PARTY DBC의 68개 신호 정의 대조.
- **실차 작동 검증은 미완료**입니다. 송신 성공은 차량의 요청 수락을 의미하지 않습니다.

재빌드:

```sh
ARDUINO_CLI=/path/to/arduino-cli \
ARDUINO_CONFIG_FILE=/path/to/arduino.yaml \
  ./tools/build_convenience_firmware.sh /tmp/t2can-convenience-firmware
```

상세 자료: [기능 통합 검토](docs/convenience-port-review.md),
[DBC 검토와 남은 확인 사항](docs/juniper-dbc-review.md).
