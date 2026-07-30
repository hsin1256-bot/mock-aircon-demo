# 모의 에어컨(Mock Aircon) 시연 프로젝트

`esp32-env-power-dashboard` 프로젝트의 공모전 시연 장소에 실제 에어컨/리모컨이 없어서, 송신부가 보내는 IR 신호를 받아 "어떤 명령인지" 해석하고 화면에 보여주는 독립된 보조 프로젝트다. **이 저장소는 esp32-env-power-dashboard와 완전히 분리되어 있으며, 자체 백엔드로 단독 실행된다.**

## 구성

```
mock-aircon-demo/
├── firmware/mock_aircon_receiver/   # XIAO ESP32-S3용 IR 수신·판정 펌웨어
│   ├── mock_aircon_receiver.ino
│   ├── irCodes.h                    # esp32-env-power-dashboard에서 동기화한 사본 (아래 참고)
│   └── secrets.h.example
├── backend/                          # 독립 Express + WebSocket 서버
│   ├── server.js
│   ├── package.json
│   └── .env.example
└── frontend/                         # 판정 결과를 보여주는 정적 페이지
    ├── mock-aircon.html
    ├── css/style.css
    └── js/mock-aircon.js
```

## esp32-env-power-dashboard와의 관계

이 프로젝트는 독립적으로 실행되지만, 딱 한 가지 **논리적 의존성**이 있다 — 송신부가 어떤 IR 신호를 보내는지 알아야 그걸 해석할 수 있다.

- `firmware/mock_aircon_receiver/irCodes.h`는 `esp32-env-power-dashboard/firmware/esp32_sensor_node/irCodes.h`를 **수동으로 동기화한 사본**이다. 두 저장소가 분리되어 있어 예전처럼 상대경로 include로 자동 동기화할 수 없다 — 송신부에서 리모컨을 다시 캡처하면 **이 파일도 손으로 다시 복사해서 맞춰야 한다.** 어긋나면 판정이 항상 `UNKNOWN`으로 나온다.
- `DEVICE_TOKEN`은 esp32-env-power-dashboard의 것과 **무관한 별도 값**이다. 이 프로젝트의 `backend/.env`와 `firmware/mock_aircon_receiver/secrets.h`끼리만 일치하면 된다.

## 하드웨어

Seeed Studio XIAO ESP32-S3 + IR 수신 모듈 1개.

- **권장(케이스 A)**: 복조 IC 내장 모듈(KY-022, VS1838B, TSOP38238 등) — 3핀(S/VCC/GND)만 연결.
- **대안(케이스 B)**: 복조 IC 없는 포토트랜지스터(PT334-6B 등) — 5단 디스크리트 복조 회로 필요(esp32-env-power-dashboard의 `CLAUDE.md` PT334-6B 섹션 참고).

## 실행 방법

### 1. 백엔드

```bash
cd backend
npm install
cp .env.example .env
# .env의 DEVICE_TOKEN을 정하고 아래 펌웨어 secrets.h와 동일하게 맞춘다
npm start
```

`http://localhost:<PORT>/mock-aircon.html`로 접속하면 판정 결과 화면이 뜬다.

### 2. 펌웨어

```bash
cd firmware/mock_aircon_receiver
cp secrets.h.example secrets.h
# WIFI_SSID/PASSWORD, SERVER_HOST(위 백엔드가 도는 IP), DEVICE_TOKEN 채우기

cd ../..
arduino-cli lib install "IRremoteESP8266" "ArduinoJson" "WebSockets"
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:CDCOnBoot=default firmware/mock_aircon_receiver
arduino-cli upload -p <포트> --fqbn esp32:esp32:XIAO_ESP32S3:CDCOnBoot=default firmware/mock_aircon_receiver
```

### 3. 하드웨어 없이 로컬 검증

```bash
cd backend
node -e "
const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:4000/ws?token=<DEVICE_TOKEN 값>');
ws.on('open', () => {
  ws.send(JSON.stringify({
    type: 'mock_aircon_state',
    device_id: 'ESP32-MOCK-AIRCON-01',
    matched: true,
    status: 'ON',
    temp: 22,
    raw: [9024, 4512, 564, 564, 564, 1692]
  }));
  setTimeout(() => process.exit(0), 500);
});
"
curl -s http://localhost:4000/api/mock-aircon
```
