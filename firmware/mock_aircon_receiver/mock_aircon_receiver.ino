/**
 * ============================================================
 * 파일 경로 : firmware/mock_aircon_receiver/mock_aircon_receiver.ino
 * ------------------------------------------------------------
 * 대상 보드: Seeed Studio XIAO ESP32-S3
 *
 * [독립 프로젝트] 이 프로젝트(mock-aircon-demo)는 esp32-env-power-
 * dashboard 본 프로젝트와 완전히 분리된 별도 저장소다. 본 프로젝트의
 * 송신부(esp32_sensor_node.ino)가 실제로 보내는 IR 신호를 받아서
 * "어떤 명령인지" 해석하고, 그 결과를 이 프로젝트 자체의 독립된
 * 백엔드(backend/server.js)로 WebSocket 전송하는 모의(가상) 에어컨
 * 수신기다. 본 프로젝트의 코드/회로는 전혀 건드리지 않는다 - 이 폴더는
 * 순전히 "받아서 해석하는" 역할만 한다.
 *
 * ⚠️ irCodes.h 동기화 필수: 이 프로젝트는 본 프로젝트와 별도 저장소라
 * irCodes.h를 상대경로로 공유할 수 없다. 그래서 이 폴더에 irCodes.h
 * 사본을 따로 둔다 - 본 프로젝트(esp32-env-power-dashboard)의
 * firmware/esp32_sensor_node/irCodes.h를 리모컨 재캡처 등으로 갱신할
 * 때마다, 이 파일도 손으로 다시 복사해서 맞춰줘야 한다. 두 파일이
 * 어긋나면 매칭이 항상 실패(UNKNOWN)한다.
 *
 * 하드웨어 연결 (IR_RECV_PIN 기준):
 *   - 케이스 A(권장): 복조 IC 내장 모듈(KY-022/VS1838B/TSOP38238 등)
 *     3핀을 그대로 연결 - S(신호)→IR_RECV_PIN, VCC→3.3~5V, GND→GND.
 *   - 케이스 B(대안): 복조 IC가 없는 포토트랜지스터(PT334-6B 등)라면
 *     별도 5단 디스크리트 복조 회로(풀업→AC결합→증폭→정류/포락선검출→
 *     정형화)를 먼저 구성해야 하고, 그 최종 출력(정형화단 컬렉터)을
 *     IR_RECV_PIN에 연결한다. 상세 회로는 CLAUDE.md의 PT334-6B
 *     섹션과 완전히 동일하다.
 *   - 어느 케이스든 최종적으로 IR_RECV_PIN에는 "active-low 디지털
 *     신호"가 들어오므로, 이후 소프트웨어 로직은 두 케이스가 동일하다.
 *
 * 필요 라이브러리:
 *   - IRremoteESP8266 (by David Conran)
 *   - ArduinoJson (by Benoit Blanchon)
 *   - WebSockets (by Markus Sattler / Links2004)
 *
 * 매칭 로직 (핵심):
 *   같은 폴더의 irCodes.h(본 프로젝트에서 동기화해온 사본)에 저장된
 *   IR_CODE_OFF / IR_TEMP_CODES와 지금 수신한 raw 배열을 항목별로
 *   비교해서 어떤 명령이었는지 판정한다.
 *
 * 신뢰성 확보: 판정 결과만 보내지 않고 raw 배열도 그대로 같이
 * 전송해서, 프론트엔드가 "원본 데이터 → 해석 결과"를 나란히
 * 보여줄 수 있게 한다.
 * ============================================================
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRutils.h>

// WIFI_SSID / WIFI_PASSWORD / SERVER_HOST / SERVER_PORT / SERVER_WS_PATH /
// DEVICE_TOKEN은 secrets.h에 정의된다(이 폴더 전용, .gitignore 대상).
#include "secrets.h"

// 본 프로젝트(esp32-env-power-dashboard)의 irCodes.h를 동기화해온
// 사본이다 - 리모컨을 다시 캡처하면 이 파일도 손으로 다시 복사해서
// 맞춰야 한다(파일 상단 경고 참고).
#include "irCodes.h"

const char* DEVICE_ID = "ESP32-MOCK-AIRCON-01";

// ---------------- 핀 정의 (XIAO ESP32-S3) ----------------
const uint16_t IR_RECV_PIN = 6;  // D5 - 케이스 A/B 공통 최종 출력단

// ---------------- IR 수신 설정 ----------------
const uint16_t kCaptureBufferSize = 1024;  // ir_scan_tool.ino와 동일한 여유값
#if DECODE_AC
const uint8_t kTimeout = 50;   // 에어컨 프레임은 길어서 넉넉하게
#else
const uint8_t kTimeout = 15;
#endif
const uint16_t kMinUnknownSize = 12;  // 노이즈로 무시할 최소 신호 길이(us)

// 캡처 회로가 케이스 B(디스크리트 복조)라면 신호에 지터가 더 낄 수 있어
// 매칭 실패가 잦으면 이 값을 30~40 정도로 올려서 재시도해본다.
const uint8_t MATCH_TOLERANCE_PERCENT = 25;

IRrecv irrecv(IR_RECV_PIN, kCaptureBufferSize, kTimeout, true);
decode_results results;

// ---------------- 네트워크 ----------------
WebSocketsClient webSocket;

const unsigned long WIFI_CHECK_INTERVAL_MS    = 3000;
const unsigned long WIFI_RECONNECT_TIMEOUT_MS = 10000;

unsigned long lastWifiCheckMillis      = 0;
unsigned long wifiReconnectStartMillis = 0;
bool wifiReconnecting = false;

// ---------------- 매칭 결과 ----------------
struct MatchResult {
  bool found;   // irCodes.h에 있는 값 중 하나와 일치했는가
  bool isOn;    // found==true일 때만 유효 (true=ON, false=OFF)
  int  tempC;   // found==true && isOn==true일 때만 유효
};

// ============================================================
// 함수 프로토타입
// ============================================================
void connectWiFi();
void handleWiFiReconnect();
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
bool rawArraysMatch(const uint16_t* received, uint16_t receivedLen,
                     const uint16_t* known, uint16_t knownLen);
MatchResult matchAgainstKnownCodes(const uint16_t* raw, uint16_t rawLen);
void handleCapturedSignal();
void sendMockAirconState(const MatchResult& match, const uint16_t* raw, uint16_t rawLen);

// ============================================================
// setup()
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  irrecv.setUnknownThreshold(kMinUnknownSize);
  irrecv.enableIRIn();

  connectWiFi();

  String wsPath = String(SERVER_WS_PATH) + "?token=" + String(DEVICE_TOKEN);
  webSocket.begin(SERVER_HOST, SERVER_PORT, wsPath);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(3000);

  Serial.println("[Mock-Aircon] 대기 중 - 송신부의 IR 신호를 기다립니다.");
}

// ============================================================
// loop()
// ============================================================
void loop() {
  webSocket.loop();

  unsigned long now = millis();
  if (now - lastWifiCheckMillis >= WIFI_CHECK_INTERVAL_MS) {
    lastWifiCheckMillis = now;
    handleWiFiReconnect();
  }

  if (irrecv.decode(&results)) {
    handleCapturedSignal();
    irrecv.resume();
  }
}

// ------------------------------------------------------------
// Wi-Fi 연결 (esp32_sensor_node.ino와 동일한 패턴)
// ------------------------------------------------------------
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void handleWiFiReconnect() {
  if (WiFi.status() == WL_CONNECTED) {
    if (wifiReconnecting) {
      Serial.println("[WiFi] Reconnected.");
      wifiReconnecting = false;
    }
    return;
  }

  if (!wifiReconnecting) {
    Serial.println("[WiFi] Connection lost. Starting reconnection...");
    wifiReconnecting = true;
    wifiReconnectStartMillis = millis();
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    return;
  }

  if (millis() - wifiReconnectStartMillis >= WIFI_RECONNECT_TIMEOUT_MS) {
    Serial.println("[WiFi] Reconnection timeout. Retrying...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiReconnectStartMillis = millis();
  }
}

// ------------------------------------------------------------
// WebSocket 이벤트 콜백. 이 보드는 서버로부터 명령을 받을 일이 없어
// (수신·판정 전용) 연결 상태 로그만 남긴다.
// ------------------------------------------------------------
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("[WS] Connected.");
      break;
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected - library will auto-reconnect.");
      break;
    default:
      break;
  }
}

// ------------------------------------------------------------
// raw 배열 두 개가 (허용 오차 내에서) 같은 신호인지 비교한다.
// irrecv.match()는 IRremoteESP8266이 자체 프로토콜 디코더에서 쓰는
// 것과 동일한 공차 비교 함수라, 우리가 직접 오차 계산을 구현할
// 필요가 없다.
// ------------------------------------------------------------
bool rawArraysMatch(const uint16_t* received, uint16_t receivedLen,
                     const uint16_t* known, uint16_t knownLen) {
  if (receivedLen != knownLen) return false;
  for (uint16_t i = 0; i < receivedLen; i++) {
    if (!irrecv.match(received[i], known[i], MATCH_TOLERANCE_PERCENT)) return false;
  }
  return true;
}

// ------------------------------------------------------------
// 수신한 raw 신호를 irCodes.h의 IR_CODE_OFF / IR_TEMP_CODES와 차례로
// 비교해 어떤 명령이었는지 판정한다. 전부 불일치하면 found=false
// (판정 불가 - UNKNOWN)를 반환한다.
// ------------------------------------------------------------
MatchResult matchAgainstKnownCodes(const uint16_t* raw, uint16_t rawLen) {
  MatchResult result = { false, false, 0 };

  if (IR_CODE_OFF_LEN > 0 && rawArraysMatch(raw, rawLen, IR_CODE_OFF, IR_CODE_OFF_LEN)) {
    result.found = true;
    result.isOn = false;
    return result;
  }

  for (uint16_t i = 0; i < IR_TEMP_CODES_COUNT; i++) {
    const IrTempCode& entry = IR_TEMP_CODES[i];
    if (entry.len == 0) continue;  // irCodes.h에 미캡처 상태면 건너뜀

    if (rawArraysMatch(raw, rawLen, entry.code, entry.len)) {
      result.found = true;
      result.isOn = true;
      result.tempC = entry.tempC;
      return result;
    }
  }

  return result;
}

// ------------------------------------------------------------
// IRrecv가 신호 하나를 캡처했을 때 호출된다. raw 배열을 뽑아
// 판정하고, 판정 결과 + raw 배열을 그대로 서버로 전송한다.
// ------------------------------------------------------------
void handleCapturedSignal() {
  uint16_t rawLen = getCorrectedRawLength(&results);
  uint16_t* raw = resultToRawArray(&results);
  if (raw == nullptr) {
    Serial.println("[Mock-Aircon] raw 배열 메모리 할당 실패, 이번 캡처 스킵.");
    return;
  }

  MatchResult match = matchAgainstKnownCodes(raw, rawLen);

  if (match.found) {
    if (match.isOn) {
      Serial.printf("[Mock-Aircon] 판정: ON, %d C (길이 %d)\n", match.tempC, rawLen);
    } else {
      Serial.printf("[Mock-Aircon] 판정: OFF (길이 %d)\n", rawLen);
    }
  } else {
    Serial.printf("[Mock-Aircon] 판정: UNKNOWN - irCodes.h의 캡처값과 불일치 (길이 %d)\n", rawLen);
  }

  sendMockAirconState(match, raw, rawLen);

  delete[] raw;  // resultToRawArray()가 new[]로 할당하므로 delete[]로 해제
}

// ------------------------------------------------------------
// 판정 결과를 WebSocket으로 전송한다. raw 배열을 그대로 실어보내는
// 이유는 프론트엔드가 "원본 타이밍 데이터"와 "해석된 값"을 나란히
// 보여줘야 판정 과정이 투명하게 증명되기 때문이다.
// ------------------------------------------------------------
void sendMockAirconState(const MatchResult& match, const uint16_t* raw, uint16_t rawLen) {
  DynamicJsonDocument doc(JSON_OBJECT_SIZE(6) + JSON_ARRAY_SIZE(rawLen) + rawLen * sizeof(uint16_t) + 128);

  doc["type"]      = "mock_aircon_state";
  doc["device_id"] = DEVICE_ID;
  doc["matched"]   = match.found;
  doc["status"]    = match.found ? (match.isOn ? "ON" : "OFF") : "UNKNOWN";
  if (match.found && match.isOn) {
    doc["temp"] = match.tempC;
  }

  JsonArray rawArray = doc.createNestedArray("raw");
  for (uint16_t i = 0; i < rawLen; i++) {
    rawArray.add(raw[i]);
  }

  String payload;
  serializeJson(doc, payload);
  webSocket.sendTXT(payload);
}
