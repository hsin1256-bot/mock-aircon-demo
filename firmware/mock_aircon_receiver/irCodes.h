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
 * 정확하다(상태 추적/오차 누적 없음). 대신 시연에 쓸 온도 개수만큼
 * 반복 캡처해야 한다 — 전 온도(16~30)를 다 캡처할 필요는 없다.
 *
 * 캡처할 온도를 20~24도로 잡은 이유: 불쾌지수 역산 추천 알고리즘
 * (backend/services/airconController.js의 recommendTemperature(),
 * REDESIGN.md 1번 참고)이 실내 습도 30~90% 전 구간에서 뽑아내는 값이
 * 실제로 이 5도 구간에 거의 다 들어온다. 예전처럼 24/26/28도만
 * 캡처해두면 추천값과 겹치는 게 24도 하나뿐이라, sendIRCommand()의
 * "가장 가까운 캡처 온도로 대체" 로직이 거의 항상 24도로 수렴해버려
 * 추천 알고리즘이 사실상 무의미해진다.
 *
 * 목표 온도가 캡처해둔 온도와 정확히 일치하지 않으면, sendIRCommand()가
 * 캡처된 온도 중 가장 가까운 값으로 자동 대체해서 보낸다.
 *
 * 캡처 절차:
 *   1) firmware/ir_scan_tool/ir_scan_tool.ino를 먼저 돌려서 이 리모컨의
 *      프로토콜이 IRremoteESP8266에 이미 지원되는지 확인한다(지원되면
 *      raw capture 없이 IRxxxAc 상태 클래스를 바로 쓰는 게 낫다).
 *      UNKNOWN으로 나올 때만 아래 raw capture 절차를 따른다.
 *   2) IR 수신 모듈(TSOP38238 등)을 ESP32에 임시로 연결한다.
 *   3) Arduino IDE에서 File > Examples > IRremoteESP8266 >
 *      IRrecvDumpV2 예제를 업로드한다(또는 ir_scan_tool.ino 재사용).
 *   4) 실제 리모컨으로 "냉방 ON, 20도" 버튼을 누르면 시리얼 모니터에
 *        uint16_t rawData[71] = {3060, 1590, 420, 480, ...};
 *      형태로 출력된다. 이 배열 내용과 길이(위 예시의 71)를 그대로
 *      IR_CODE_ON_20 / IR_CODE_ON_20_LEN에 옮겨 적는다.
 *   5) 21~24도도 같은 방식으로 캡처해 IR_CODE_ON_21~24에 옮긴다
 *      (이미 IR_TEMP_CODES 배열에 5개 항목이 다 준비돼 있으니 값만
 *      채우면 된다).
 *   6) 같은 방식으로 "전원 OFF" 신호를 캡처해 IR_CODE_OFF /
 *      IR_CODE_OFF_LEN에 옮긴다.
 *   7) IRrecvDumpV2가 함께 출력하는 캐리어 주파수(대부분 38kHz)를
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

// ---- 냉방 ON 신호 (온도별로 전체 상태를 통째로 캡처) ----
// TODO: 캡처한 rawData[] 배열 값으로 전체 교체. 길이가 0인 항목은
// 미캡처로 간주되어 IR_TEMP_CODES에서 제외된다.
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

// 다른 온도를 추가하고 싶으면 위처럼 IR_CODE_ON_xx 배열을 하나 더
// 캡처해 추가하고, 아래 IR_TEMP_CODES에도 항목을 추가한다.
struct IrTempCode {
  int tempC;
  const uint16_t* code;
  uint16_t len;
};

const IrTempCode IR_TEMP_CODES[] = {
  { 20, IR_CODE_ON_20, IR_CODE_ON_20_LEN },
  { 21, IR_CODE_ON_21, IR_CODE_ON_21_LEN },
  { 22, IR_CODE_ON_22, IR_CODE_ON_22_LEN },
  { 23, IR_CODE_ON_23, IR_CODE_ON_23_LEN },
  { 24, IR_CODE_ON_24, IR_CODE_ON_24_LEN },
};
const uint16_t IR_TEMP_CODES_COUNT = sizeof(IR_TEMP_CODES) / sizeof(IR_TEMP_CODES[0]);

// ---- 전원 OFF 신호 ----
// TODO: 캡처한 rawData[] 배열 값으로 전체 교체
const uint16_t IR_CODE_OFF[] = {0};
// TODO: 캡처한 배열의 실제 길이로 교체 (0이면 미캡처로 간주해 전송을 건너뜀)
const uint16_t IR_CODE_OFF_LEN = 0;

#endif
