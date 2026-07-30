/**
 * ============================================================
 * 파일 경로: frontend/js/mock-aircon.js
 * ------------------------------------------------------------
 * GET /api/mock-aircon을 2초마다 폴링해 모의 에어컨 판정 결과를
 * 표시한다. 주기를 짧게 잡은 이유는 시연 중 "즉시 반응한다"는 인상이
 * 신뢰성에 중요하기 때문이다.
 * ============================================================
 */

const REFRESH_INTERVAL_MS = 2000;
const MOCK_AIRCON_API_URL = '/api/mock-aircon';

function formatStatus(status) {
  if (status === 'ON' || status === 'OFF' || status === 'UNKNOWN') return status;
  return '대기 중';
}

function updateUI(data) {
  const statusEl  = document.getElementById('mock-status');
  const badgeEl   = document.getElementById('mock-badge');
  const tempEl    = document.getElementById('mock-temp');
  const rawEl     = document.getElementById('mock-raw');
  const updatedEl = document.getElementById('mock-updated');

  statusEl.textContent = formatStatus(data.status);

  badgeEl.classList.remove('on', 'off', 'unknown');
  if (data.status === 'ON') badgeEl.classList.add('on');
  else if (data.status === 'OFF') badgeEl.classList.add('off');
  else if (data.status === 'UNKNOWN') badgeEl.classList.add('unknown');

  tempEl.textContent = typeof data.temp === 'number' ? `${data.temp}℃` : '--';

  rawEl.textContent = Array.isArray(data.raw) && data.raw.length > 0
    ? `[${data.raw.join(', ')}]`
    : '(아직 수신된 신호 없음)';

  updatedEl.textContent = data.updated_at
    ? `마지막 수신: ${new Date(data.updated_at * 1000).toLocaleTimeString('ko-KR')}`
    : '마지막 수신: -';
}

async function fetchMockAirconState() {
  try {
    const res = await fetch(MOCK_AIRCON_API_URL, { cache: 'no-store' });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    updateUI(data);
  } catch (err) {
    console.error('[MockAircon] Failed to fetch state:', err);
  }
}

fetchMockAirconState();
setInterval(fetchMockAirconState, REFRESH_INTERVAL_MS);
