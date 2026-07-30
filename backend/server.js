/**
 * ============================================================
 * 파일 경로: backend/server.js
 * ------------------------------------------------------------
 * 모의 에어컨(mock aircon) 전용 독립 서버. esp32-env-power-dashboard
 * 프로젝트의 백엔드와는 완전히 별개로 실행된다(같은 DB/설정/토큰을
 * 공유하지 않음). 하는 일은 단 두 가지뿐이다:
 *
 *   1) firmware/mock_aircon_receiver 보드가 WebSocket으로 보내는
 *      "mock_aircon_state" 판정 결과를 받아 메모리 캐시에 저장한다
 *      (DB 없음 - 이력이 아니라 "지금 값"이라 서버 재시작 시 초기화됨).
 *   2) GET /api/mock-aircon으로 그 캐시를 그대로 반환하고,
 *      frontend/ 정적 파일(모의 에어컨 페이지)을 서빙한다.
 *
 * 인증은 esp32-env-power-dashboard의 WebSocket과 동일한 방식
 * (접속 URL 쿼리 파라미터 ?token=DEVICE_TOKEN)을 그대로 재사용한다.
 * ============================================================
 */
require('dotenv').config();
const path = require('path');
const http = require('http');
const crypto = require('crypto');
const express = require('express');
const { WebSocketServer } = require('ws');

const PORT = process.env.PORT || 4000;
const DEVICE_TOKEN = process.env.DEVICE_TOKEN;

// 모의 에어컨 판정 결과 캐시. DB에 저장하지 않는 "지금 값"이다 -
// 서버 재시작 시 초기화되고, 다음 수신 시 다시 채워진다.
const latestMockAircon = {
  status: null,       // "ON" | "OFF" | "UNKNOWN" | null(아직 한 번도 수신 안 됨)
  temp: null,          // status가 "ON"일 때만 숫자, 그 외 null
  matched: false,      // irCodes.h의 캡처값과 실제로 일치했는지
  raw: [],              // 수신한 raw 타이밍 배열 그대로 (신뢰성 확보용)
  updated_at: null,
};

// timingSafeEqual은 두 버퍼의 길이가 다르면 예외를 던지므로, 길이가
// 다를 때는(더미 버퍼끼리) 비교를 한 번 수행해 실행 시간 패턴을
// 비슷하게 유지한 뒤 false를 반환한다.
function timingSafeEqualString(a, b) {
  const bufA = Buffer.from(String(a));
  const bufB = Buffer.from(String(b));
  if (bufA.length !== bufB.length) {
    crypto.timingSafeEqual(bufA, bufA);
    return false;
  }
  return crypto.timingSafeEqual(bufA, bufB);
}

function handleMockAirconState(msg) {
  const validStatus = msg.status === 'ON' || msg.status === 'OFF' || msg.status === 'UNKNOWN';
  if (!validStatus || !Array.isArray(msg.raw)) {
    console.warn('[WS] mock_aircon_state message malformed, ignored:', msg);
    return;
  }

  latestMockAircon.status = msg.status;
  latestMockAircon.temp = (msg.status === 'ON' && Number.isInteger(msg.temp)) ? msg.temp : null;
  latestMockAircon.matched = msg.matched === true;
  latestMockAircon.raw = msg.raw;
  latestMockAircon.updated_at = Math.floor(Date.now() / 1000);

  const tempLabel = latestMockAircon.temp !== null ? ` ${latestMockAircon.temp}C` : '';
  console.log(`[WS] mock_aircon_state: ${msg.status}${tempLabel} (matched=${latestMockAircon.matched})`);
}

const app = express();
app.use(express.static(path.join(__dirname, '..', 'frontend')));

app.get('/api/mock-aircon', (req, res) => {
  return res.status(200).json({ ...latestMockAircon, raw: [...latestMockAircon.raw] });
});

app.use((err, req, res, next) => {
  console.error('[Unhandled Error]', err);
  res.status(500).json({ error: 'Internal server error' });
});

const server = http.createServer(app);

const wss = new WebSocketServer({
  server,
  path: '/ws',
  verifyClient(info, callback) {
    if (!DEVICE_TOKEN) {
      console.error('[WS] DEVICE_TOKEN is not configured on server (.env 확인).');
      callback(false, 500, 'Server misconfiguration');
      return;
    }

    const url = new URL(info.req.url, 'http://localhost');
    const token = url.searchParams.get('token');
    if (!token || !timingSafeEqualString(token, DEVICE_TOKEN)) {
      callback(false, 401, 'Invalid or missing token');
      return;
    }
    callback(true);
  },
});

wss.on('connection', (ws) => {
  console.log('[WS] Device connected.');

  ws.on('message', (raw) => {
    let msg;
    try {
      msg = JSON.parse(raw.toString());
    } catch (err) {
      console.error('[WS] Invalid JSON from device:', err.message);
      return;
    }

    if (msg.type === 'mock_aircon_state') {
      handleMockAirconState(msg);
    } else {
      console.warn('[WS] Unknown message type:', msg.type);
    }
  });

  ws.on('close', () => console.log('[WS] Device disconnected.'));
  ws.on('error', (err) => console.error('[WS] Socket error:', err.message));
});

server.listen(PORT, () => {
  console.log(`[Server] Listening on port ${PORT} (HTTP + WebSocket /ws)`);
});
