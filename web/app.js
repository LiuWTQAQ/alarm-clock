const DEV = 'a943f8';
const $ = s => document.querySelector(s);
const $$ = s => [...document.querySelectorAll(s)];

const CITIES = [
  ['北京','Asia/Shanghai'],['上海','Asia/Shanghai'],['广州','Asia/Shanghai'],['深圳','Asia/Shanghai'],
  ['成都','Asia/Shanghai'],['重庆','Asia/Shanghai'],['杭州','Asia/Shanghai'],['南京','Asia/Shanghai'],
  ['武汉','Asia/Shanghai'],['西安','Asia/Shanghai'],['天津','Asia/Shanghai'],['苏州','Asia/Shanghai'],
  ['长沙','Asia/Shanghai'],['郑州','Asia/Shanghai'],['青岛','Asia/Shanghai'],['大连','Asia/Shanghai'],
  ['厦门','Asia/Shanghai'],['昆明','Asia/Shanghai'],['哈尔滨','Asia/Shanghai'],['乌鲁木齐','Asia/Shanghai'],
  ['香港','Asia/Hong_Kong'],['台北','Asia/Taipei'],['东京','Asia/Tokyo'],['首尔','Asia/Seoul'],
  ['新加坡','Asia/Singapore'],['曼谷','Asia/Bangkok'],['迪拜','Asia/Dubai'],['伦敦','Europe/London'],
  ['巴黎','Europe/Paris'],['柏林','Europe/Berlin'],['莫斯科','Europe/Moscow'],['纽约','America/New_York'],
  ['芝加哥','America/Chicago'],['洛杉矶','America/Los_Angeles'],['悉尼','Australia/Sydney'],
];

const TZS = ['Asia/Shanghai','Asia/Hong_Kong','Asia/Taipei','Asia/Tokyo','Asia/Seoul','Asia/Singapore',
  'Asia/Bangkok','Asia/Dubai','Asia/Kolkata','Europe/London','Europe/Paris','Europe/Berlin',
  'Europe/Moscow','America/New_York','America/Chicago','America/Denver','America/Los_Angeles',
  'America/Sao_Paulo','Australia/Sydney','Pacific/Auckland','UTC'];

const MASK_TEXT = {0:'一次', 31:'工作日', 96:'周末', 127:'每天'};
const rep = m => MASK_TEXT[m] || '自定义';

let state = {}, settings = {}, editing = null, view = 'home';

async function api(path, opts = {}) {
  const r = await fetch(path, {headers: {'Content-Type': 'application/json'}, ...opts});
  if (!r.ok) throw new Error(`${r.status} ${path}`);
  return r.json();
}

/* ---------------------------------------------------------- routing */
function route() {
  view = location.hash === '#/settings' ? 'settings' : 'home';
  $('#view-home').hidden = view !== 'home';
  $('#view-settings').hidden = view !== 'settings';
  $$('.tab').forEach(t => t.classList.toggle('active', t.dataset.view === view));
  if (view === 'settings') loadSettings();
}
window.addEventListener('hashchange', route);

/* ---------------------------------------------------------- clock */
function paintClock() {
  const now = new Date();
  const p = n => String(n).padStart(2, '0');
  let h = now.getHours(), ap = '';
  if (settings.hour12) { ap = h < 12 ? 'AM' : 'PM'; h = h % 12 || 12; }
  $('#clock').textContent = `${settings.hour12 ? h : p(h)}:${p(now.getMinutes())}:${p(now.getSeconds())}`;
  $('#ampm').textContent = ap;
}
setInterval(paintClock, 1000);

/* ---------------------------------------------------------- home */
function paintState(s) {
  state = s;
  $('#dot').className = 'dot' + (s.online ? ' on' : '');
  $('#conn').textContent = s.online ? `在线 · 信号 ${s.net?.rssi ?? '--'} dBm` : '离线（使用本地闹钟）';
  const f = !!settings.fahrenheit;
  const t = s.env?.t;
  $('#temp').textContent = t == null ? '--' : (f ? Math.round(t * 9 / 5 + 32) : t);
  $('#unit').textContent = f ? '°F' : '°C';
  $('#cond').textContent = s.cond || (s.env?.cond ?? '—');
  $('#place').textContent = `${settings.city ?? '—'} · 空气 ${s.env?.aqi ?? '--'}`;
  const lampOn = !!s.light?.on;
  $('#lamp').className = 'toggle ' + (lampOn ? 'on' : 'off');
  $('#lamp').textContent = lampOn ? '开' : '关';
  const acOn = !!s.ac?.on;
  $('#ac').className = 'toggle ' + (acOn ? 'on' : 'off');
  $('#ac').textContent = acOn ? '开' : '关';
  $('#actemp').textContent = `${s.ac?.temp ?? 26} °C`;
}

function paintAlarms(rows) {
  const ul = $('#alarms');
  ul.innerHTML = '';
  if (!rows.length) { ul.innerHTML = '<li class="muted">还没有闹钟</li>'; return; }
  for (const a of rows) {
    const li = document.createElement('li');
    const hh = settings.hour12 ? (a.h % 12 || 12) : String(a.h).padStart(2, '0');
    li.innerHTML = `<span class="time">${hh}:${String(a.m).padStart(2,'0')}${
      settings.hour12 ? `<small class="muted"> ${a.h < 12 ? 'AM' : 'PM'}</small>` : ''}</span>
      <span class="rep">${a.label || ''} ${rep(a.mask)}</span>`;
    const t = document.createElement('button');
    t.className = 'toggle ' + (a.on ? 'on' : 'off');
    t.textContent = a.on ? '开' : '关';
    t.onclick = async () => {
      await api(`/api/devices/${DEV}/alarms/${a.id}`, {method: 'PUT', body: JSON.stringify(
        {h: a.h, m: a.m, mask: a.mask, on: !a.on, label: a.label})});
      refresh();
    };
    const edit = document.createElement('button');
    edit.className = 'ghost'; edit.textContent = '改';
    edit.onclick = () => openDialog(a);
    const del = document.createElement('button');
    del.className = 'ghost'; del.textContent = '删';
    del.onclick = async () => {
      await api(`/api/devices/${DEV}/alarms/${a.id}`, {method: 'DELETE'});
      refresh();
    };
    li.append(t, edit, del);
    ul.append(li);
  }
}

async function refresh() {
  try {
    const [devs, s, alarms] = await Promise.all([
      api('/api/devices'), api(`/api/devices/${DEV}/state`),
      api(`/api/devices/${DEV}/alarms`)]);
    if (devs[0]) $('#devName').textContent = devs[0].name;
    paintState(s); paintAlarms(alarms);
    const ev = await api(`/api/devices/${DEV}/events?limit=6`);
    $('#events').innerHTML = ev.map(e =>
      `<li>${new Date(e.ts * 1000).toLocaleTimeString()} · ${e.type}</li>`).join('')
      || '<li>暂无事件</li>';
  } catch { $('#conn').textContent = '服务未启动'; }
}

/* ---------------------------------------------------------- settings */
async function loadSettings() {
  try {
    settings = await api(`/api/devices/${DEV}/settings`);
  } catch { settings = {}; }
  if (!$('#sCity').options.length) {
    $('#sCity').innerHTML = CITIES.map(([c]) => `<option>${c}</option>`).join('');
    $('#sTz').innerHTML = TZS.map(t => `<option>${t}</option>`).join('');
  }
  $('#sCity').value = settings.city ?? '成都';
  $('#sTz').value = settings.tz ?? 'Asia/Shanghai';
  $('#sHour').value = settings.hour12 ? '12' : '24';
  $('#sUnit').value = settings.fahrenheit ? 'f' : 'c';
  $('#sWx').value = String(settings.wx_interval ?? 30);
  $('#sName').value = settings.name ?? $('#devName').textContent;
  paintClock(); paintState(state); 
}

$('#sCity').addEventListener('change', () => {
  const hit = CITIES.find(([c]) => c === $('#sCity').value);
  if (hit) $('#sTz').value = hit[1];
});

$('#saveSettings').onclick = async () => {
  const body = {
    city: $('#sCity').value,
    tz: $('#sTz').value,
    hour12: $('#sHour').value === '12',
    fahrenheit: $('#sUnit').value === 'f',
    wx_interval: Number($('#sWx').value),
    name: $('#sName').value.trim() || '卧室闹钟',
  };
  $('#saveHint').textContent = '保存中…';
  try {
    const r = await api(`/api/devices/${DEV}/settings`, {method: 'PUT', body: JSON.stringify(body)});
    $('#devName').textContent = r.settings.name;
    settings = r.settings;
    const pushed = Object.keys(r.pushed || {});
    $('#saveHint').textContent = pushed.length
      ? `已保存，下发到设备：${pushed.join('、')}`
      : '已保存（与设备一致，无需下发）';
    paintClock(); paintAlarms(await api(`/api/devices/${DEV}/alarms`));
  } catch (e) {
    $('#saveHint').textContent = '保存失败：' + e.message;
  }
};

/* ---------------------------------------------------------- alarms dialog */
function openDialog(a) {
  editing = a || null;
  $('#dlgTitle').textContent = a ? '编辑闹钟' : '新增闹钟';
  $('#fTime').value = a ? `${String(a.h).padStart(2,'0')}:${String(a.m).padStart(2,'0')}` : '07:30';
  $('#fMask').value = a ? a.mask : 31;
  $('#fLabel').value = a?.label || '';
  $('#dlg').showModal();
}

$('#form').addEventListener('submit', async e => {
  e.preventDefault();
  const [h, m] = $('#fTime').value.split(':').map(Number);
  const body = {h, m, mask: Number($('#fMask').value), on: true, label: $('#fLabel').value.slice(0, 9)};
  if (editing) await api(`/api/devices/${DEV}/alarms/${editing.id}`, {method: 'PUT', body: JSON.stringify(body)});
  else await api(`/api/devices/${DEV}/alarms`, {method: 'POST', body: JSON.stringify(body)});
  editing = null; refresh();
});

$('#addBtn').onclick = () => openDialog(null);
$('#lamp').onclick = async () => {
  await api(`/api/devices/${DEV}/cmd`, {method: 'POST',
    body: JSON.stringify({op: 'light.set', on: !state.light?.on})});
  setTimeout(refresh, 400);
};
$('#ac').onclick = async () => {
  await api(`/api/devices/${DEV}/cmd`, {method: 'POST',
    body: JSON.stringify({op: 'ac.send', key: 'power', code: '0x00FF00FF'})});
  setTimeout(refresh, 400);
};
const bump = async d => {
  await api(`/api/devices/${DEV}/cmd`, {method: 'POST',
    body: JSON.stringify({op: 'ac.send', key: 'temp', code: `0x00FF${d > 0 ? '01' : '00'}`})});
  setTimeout(refresh, 400);
};
$('#tmin').onclick = () => bump(-1);
$('#tplus').onclick = () => bump(+1);

/* ---------------------------------------------------------- boot */
(async function boot() {
  try { settings = await api(`/api/devices/${DEV}/settings`); } catch {}
  route();
  paintClock();
  refresh();
  setInterval(refresh, 2000);
})();
