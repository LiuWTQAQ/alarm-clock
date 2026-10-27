const DEV = 'a943f8';
const $ = s => document.querySelector(s);
let state = {}, editing = null;

const MASK_TEXT = {0:'一次', 31:'工作日', 96:'周末', 127:'每天'};
const rep = m => MASK_TEXT[m] || '自定义';

async function api(path, opts = {}) {
  const r = await fetch(path, {headers: {'Content-Type': 'application/json'}, ...opts});
  if (!r.ok) throw new Error(r.status);
  return r.json();
}

function paintClock() {
  const now = new Date();
  const p = n => String(n).padStart(2, '0');
  $('#clock').textContent = `${p(now.getHours())}:${p(now.getMinutes())}:${p(now.getSeconds())}`;
}

function paintAlarms(rows) {
  const ul = $('#alarms');
  ul.innerHTML = '';
  if (!rows.length) { ul.innerHTML = '<li class="muted">还没有闹钟</li>'; return; }
  for (const a of rows) {
    const li = document.createElement('li');
    li.innerHTML = `<span class="time">${String(a.h).padStart(2,'0')}:${String(a.m).padStart(2,'0')}</span>
      <span class="rep">${a.label || ''} ${rep(a.mask)}</span>`;
    const t = document.createElement('button');
    t.className = 'toggle ' + (a.on ? 'on' : 'off');
    t.textContent = a.on ? '开' : '关';
    t.onclick = async () => {
      await api(`/api/devices/${DEV}/alarms/${a.id}`, {method: 'PUT', body: JSON.stringify(
        {h: a.h, m: a.m, mask: a.mask, on: !a.on, label: a.label})});
      refresh();
    };
    const del = document.createElement('button');
    del.className = 'ghost'; del.textContent = '删';
    del.onclick = async () => {
      await api(`/api/devices/${DEV}/alarms/${a.id}`, {method: 'DELETE'});
      refresh();
    };
    const edit = document.createElement('button');
    edit.className = 'ghost'; edit.textContent = '改';
    edit.onclick = () => openDialog(a);
    li.append(t, edit, del);
    li.onclick = e => { if (e.target.tagName === 'LI') openDialog(a); };
    ul.append(li);
  }
}

function paintState(s) {
  state = s;
  const online = !!s.online;
  $('#dot').className = 'dot' + (online ? ' on' : '');
  $('#conn').textContent = online ? `在线 · 信号 ${s.net?.rssi ?? '--'} dBm` : '离线（使用本地闹钟）';
  if (s.env) { $('#temp').textContent = s.env.t ?? '--'; $('#aqi').textContent = s.env.aqi ?? '--'; }
  $('#cond').textContent = s.cond || '—';
  const lampOn = !!s.light?.on;
  $('#lamp').className = 'toggle ' + (lampOn ? 'on' : 'off');
  $('#lamp').textContent = lampOn ? '开' : '关';
  const acOn = !!s.ac?.on;
  $('#ac').className = 'toggle ' + (acOn ? 'on' : 'off');
  $('#ac').textContent = acOn ? '开' : '关';
  $('#actemp').textContent = `${s.ac?.temp ?? 26} °C`;
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
  } catch (e) { $('#conn').textContent = '服务未启动'; }
}

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
  const body = {h, m, mask: Number($('#fMask').value), on: true, label: $('#fLabel').value.slice(0,9)};
  if (editing) await api(`/api/devices/${DEV}/alarms/${editing.id}`, {method:'PUT', body: JSON.stringify(body)});
  else await api(`/api/devices/${DEV}/alarms`, {method:'POST', body: JSON.stringify(body)});
  editing = null; refresh();
});

$('#addBtn').onclick = () => openDialog(null);
$('#lamp').onclick = async () => {
  await api(`/api/devices/${DEV}/cmd`, {method:'POST',
    body: JSON.stringify({op:'light.set', on: !state.light?.on})});
  setTimeout(refresh, 400);
};
$('#ac').onclick = async () => {
  await api(`/api/devices/${DEV}/cmd`, {method:'POST',
    body: JSON.stringify({op:'ac.send', key:'power', code:'0x00FF00FF'})});
  setTimeout(refresh, 400);
};
const bump = async d => {
  const t = Math.max(16, Math.min(30, (state.ac?.temp ?? 26) + d));
  await api(`/api/devices/${DEV}/cmd`, {method:'POST',
    body: JSON.stringify({op:'ac.send', key:'temp', code:`0x00FF${t}`})});
  setTimeout(refresh, 400);
};
$('#tmin').onclick = () => bump(-1);
$('#tplus').onclick = () => bump(+1);

setInterval(paintClock, 1000); paintClock();
setInterval(refresh, 2000); refresh();
