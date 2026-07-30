/**
 * ============================================================
 * 파일 경로: firmware/mock_aircon_receiver/irCodes.h
 * ------------------------------------------------------------
 * ⚠️ 이 파일은 esp32-env-power-dashboard 프로젝트의
 * firmware/esp32_sensor_node/irCodes.h를 동기화해온 사본이다.
 * 원본이 갱신되면(리모컨 재캡처 등) 이 파일도 손으로 다시 복사해서
 * 맞춰야 한다 - 어긋나면 mock_aircon_receiver.ino의 매칭이 항상
 * 실패(UNKNOWN)한다.
 * ------------------------------------------------------------
 * 실제 에어컨 리모컨에서 캡처한 raw IR 신호를 담는 파일.
 * 공모전 특성상 시연에 쓸 에어컨 기종이 아직 정해지지 않아,
 * 지금은 미캡처 상태(길이 0, placeholder)다. 기종이 정해지면
 * 아래 절차로 이 파일만 채우면 되고, .ino 쪽은 손댈 필요 없다.
 *
 * 온도 조절 방식: 온도별로 "그 온도의 전체 상태"를 통째로 캡처해서
 * 재생하는 절대값 방식이다(1도씩 up/down 델타 방식이 아님). IR은
 * 단방향이라 에어컨의 현재 상태를 읽어올 수 없는데, 절대값 방식은
 * 매번 목표 온도의 신호를 그대로 재생하므로 이전에 뭘 보냈든 항상
 * 정확하다(상태 추적/오차 누적 없음).
 *
 * [변경] 캡처 범위를 20~24도(5개)에서 하드웨어가 지원하는 전체 범위인
 * **16~30도(15개)**로 확장했다. 예전엔 불쾌지수 추천값 분포에 맞춰
 * 좁게 캡처해도 충분하다고 판단했지만, 전체 범위를 다 캡처해두면
 * sendIRCommand()의 "가장 가까운 캡처 온도로 대체" 로직이 사실상
 * 항상 정확히 일치하게 되어 더 세밀한 제어가 가능해진다. 대신 캡처해야
 * 할 버튼 수가 5개에서 15개로 늘어난다(전원 OFF까지 포함하면 총 16개).
 *
 * 목표 온도가 캡처해둔 온도와 정확히 일치하지 않으면, sendIRCommand()가
 * 캡처된 온도 중 가장 가까운 값으로 자동 대체해서 보낸다(전체 범위가
 * 다 채워지면 이 대체 로직은 사실상 발동하지 않는다).
 *
 * ⚠️ 에어컨마다 실제 지원 온도 범위(최저/최고)가 다르다 — 16~30도는
 * 이 파일이 캡처를 "지원"하는 소프트웨어 상한선일 뿐, 모든 에어컨이
 * 정말로 16도까지 내려가거나 30도까지 올라가는 건 아니다(예: 18~28도
 * 까지만 되는 기종도 흔하다). 그래서 캡처를 시작하기 전에 반드시
 * 아래 0단계로 실제 범위를 먼저 확인한다. 실제 범위 밖의 온도는 그냥
 * 캡처하지 않고 비워두면 된다 — 길이 0인 항목은 자동으로 건너뛰도록
 * 이미 설계되어 있어서(IR_TEMP_CODES_COUNT 순회 시 len==0 skip),
 * 실제 지원 범위가 16~30보다 좁아도 안전하게 동작한다.
 *
 * 캡처 절차:
 *   0) [먼저] 실제 리모컨의 온도 버튼을 눌러 최저/최고 온도를 확인한다
 *      — "온도내림"을 계속 눌러 더 이상 안 내려가는 지점이 최저,
 *      "온도올림"을 계속 눌러 더 이상 안 올라가는 지점이 최고다. 이
 *      범위 밖의 IR_CODE_ON_xx 항목은 캡처하지 않고 그대로 둔다(길이
 *      0 placeholder 유지). 확인한 실제 최저/최고를
 *      backend/services/airconController.js의 MIN_TARGET_TEMP/
 *      MAX_TARGET_TEMP에도 반영해두면, 화면에 실제로 불가능한 온도가
 *      "추천 온도"로 뜨는 걸 막을 수 있다(선택 사항, 필수는 아님 -
 *      안 해도 IR 전송 자체는 아래 대체 로직 덕분에 안전하다).
 *   1) firmware/ir_scan_tool/ir_scan_tool.ino를 먼저 돌려서 이 리모컨의
 *      프로토콜이 IRremoteESP8266에 이미 지원되는지 확인한다(지원되면
 *      raw capture 없이 IRxxxAc 상태 클래스를 바로 쓰는 게 낫다).
 *      UNKNOWN으로 나올 때만 아래 raw capture 절차를 따른다.
 *   2) IR 수신 모듈(KY-022/VS1838B 등)을 ESP32에 임시로 연결한다.
 *   3) ir_scan_tool.ino를 업로드하고 시리얼 모니터(115200bps)를 연다.
 *   4) 실제 리모컨으로 "냉방 ON, 16도" 버튼을 누르면 시리얼 모니터에
 *        uint16_t rawData[71] = {3060, 1590, 420, 480, ...};
 *      형태로 출력된다. 이 배열 내용과 길이(위 예시의 71)를 그대로
 *      IR_CODE_ON_16 / IR_CODE_ON_16_LEN에 옮겨 적는다.
 *   5) 17~30도까지 같은 방식으로 캡처해 IR_CODE_ON_17~30에 옮긴다
 *      (이미 IR_TEMP_CODES 배열에 15개 항목이 다 준비돼 있으니 값만
 *      채우면 된다). 온도 버튼을 눌러 올리기만 해도 되고(전원 다시
 *      끄지 않아도 됨), 리모컨이 매번 "지금 상태 전체"를 다시 쏘는
 *      기종이면 그대로 유효한 캡처가 된다.
 *   6) 같은 방식으로 "전원 OFF" 신호를 캡처해 IR_CODE_OFF /
 *      IR_CODE_OFF_LEN에 옮긴다.
 *   7) ir_scan_tool.ino가 함께 출력하는 캐리어 주파수(대부분 38kHz)를
 *      IR_CODE_FREQUENCY_KHZ에 반영한다. 모르면 38 그대로 둔다.
 *
 * 주의: 브랜드가 확정되고 IRremoteESP8266이 그 브랜드를 상태 기반
 * 클래스(IRxxxAc)로 지원한다면, raw capture 없이 sendIRCommand()
 * 내부만 그 클래스 호출로 교체하는 게 훨씬 낫다(캡처 불필요, 16~30도
 * 전체 범위 지원, 상태 추적 불필요).
 * ============================================================
 */
#ifndef IR_CODES_H
#define IR_CODES_H

// 캐리어 주파수(kHz). 대부분의 가정용 에어컨 리모컨은 38kHz.
const uint16_t IR_CODE_FREQUENCY_KHZ = 38;

// ---- 냉방 ON 신호 (16~30도, 온도별로 전체 상태를 통째로 캡처) ----
// TODO: 캡처한 rawData[] 배열 값으로 전체 교체. 길이가 0인 항목은
// 미캡처로 간주되어 IR_TEMP_CODES에서 제외된다.
const uint16_t IR_CODE_ON_16[] = {0};
const uint16_t IR_CODE_ON_16_LEN = 0;

const uint16_t IR_CODE_ON_17[] = {0};
const uint16_t IR_CODE_ON_17_LEN = 0;

const uint16_t IR_CODE_ON_18[] = {0};
const uint16_t IR_CODE_ON_18_LEN = 0;

const uint16_t IR_CODE_ON_19[] = {0};
const uint16_t IR_CODE_ON_19_LEN = 0;

const uint16_t IR_CODE_ON_20[] = {0};
const uint16_t IR_CODE_ON_20_LEN = 0;

const uint16_t IR_CODE_ON_21[] = {0};
const uint16_t IR_CODE_ON_21_LEN = 0;

const uint16_t IR_CODE_ON_22[] = {0};
const uint16_t IR_CODE_ON_22_LEN = 0;

const uint16_t IR_CODE_ON_23[] = {0};
const uint16_t IR_CODE_ON_23_LEN = 0;

const uint16_t IR_CODE_ON_24[] = {0};
const uint16_t IR_CODE_ON_24_LEN = 0;

const uint16_t IR_CODE_ON_25[] = {0};
const uint16_t IR_CODE_ON_25_LEN = 0;

const uint16_t IR_CODE_ON_26[] = {0};
const uint16_t IR_CODE_ON_26_LEN = 0;

const uint16_t IR_CODE_ON_27[] = {0};
const uint16_t IR_CODE_ON_27_LEN = 0;

const uint16_t IR_CODE_ON_28[] = {0};
const uint16_t IR_CODE_ON_28_LEN = 0;

const uint16_t IR_CODE_ON_29[] = {0};
const uint16_t IR_CODE_ON_29_LEN = 0;

const uint16_t IR_CODE_ON_30[] = {0};
const uint16_t IR_CODE_ON_30_LEN = 0;

// 다른 온도를 추가하고 싶으면 위처럼 IR_CODE_ON_xx 배열을 하나 더
// 캡처해 추가하고, 아래 IR_TEMP_CODES에도 항목을 추가한다.
struct IrTempCode {
  int tempC;
  const uint16_t* code;
  uint16_t len;
};

const IrTempCode IR_TEMP_CODES[] = {
  { 16, IR_CODE_ON_16, IR_CODE_ON_16_LEN },
  { 17, IR_CODE_ON_17, IR_CODE_ON_17_LEN },
  { 18, IR_CODE_ON_18, IR_CODE_ON_18_LEN },
  { 19, IR_CODE_ON_19, IR_CODE_ON_19_LEN },
  { 20, IR_CODE_ON_20, IR_CODE_ON_20_LEN },
  { 21, IR_CODE_ON_21, IR_CODE_ON_21_LEN },
  { 22, IR_CODE_ON_22, IR_CODE_ON_22_LEN },
  { 23, IR_CODE_ON_23, IR_CODE_ON_23_LEN },
  { 24, IR_CODE_ON_24, IR_CODE_ON_24_LEN },
  { 25, IR_CODE_ON_25, IR_CODE_ON_25_LEN },
  { 26, IR_CODE_ON_26, IR_CODE_ON_26_LEN },
  { 27, IR_CODE_ON_27, IR_CODE_ON_27_LEN },
  { 28, IR_CODE_ON_28, IR_CODE_ON_28_LEN },
  { 29, IR_CODE_ON_29, IR_CODE_ON_29_LEN },
  { 30, IR_CODE_ON_30, IR_CODE_ON_30_LEN },
};
const uint16_t IR_TEMP_CODES_COUNT = sizeof(IR_TEMP_CODES) / sizeof(IR_TEMP_CODES[0]);

// ---- 전원 OFF 신호 ----
// TODO: 캡처한 rawData[] 배열 값으로 전체 교체
const uint16_t IR_CODE_OFF[] = {0};
// TODO: 캡처한 배열의 실제 길이로 교체 (0이면 미캡처로 간주해 전송을 건너뜀)
const uint16_t IR_CODE_OFF_LEN = 0;

#endif
