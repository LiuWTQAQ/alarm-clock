const DEV = 'a943f8';
const $ = s => document.querySelector(s);
const $$ = s => [...document.querySelectorAll(s)];

/* ---------------------------------------------------------------- i18n */
const I18N = {
  zh: {
    title:'闹钟 · 远程控制', tab_home:'主页', tab_settings:'设置', logout:'退出',
    connecting:'连接中…',
    sec_net:'网络', ssid:'Wi-Fi 名称（SSID）', wpass:'Wi-Fi 密码',
    wifi_save:'保存到设备', need_ssid:'请先填写 SSID',
    sent:'已下发，设备正在重连…',
    auto_hint:'修改后立即下发到设备', pushall:'重新下发全部设置',
    wx_updated:'天气更新', wx_never:'天气尚未更新', mins_ago:(m)=>`${m} 分钟前`,
    wifi_hint:'下发后设备会断开并重连，约 5–10 秒。填错时可在设备本地页面（屏幕上的 IP）改回。', online:(w)=>w ? `在线 · ${w}` : '在线', offline:'离线（使用本地闹钟）',
    down:'服务未启动', air:(n)=>`空气 ${n}`,
    lamp:'灯', lamp_hint:'继电器 · 卧室顶灯', ac:'空调', ac_hint:'红外 · 房间空调',
    on:'开', off:'关', alarms:'闹钟', add:'+ 新增', events:'事件',
    no_alarms:'还没有闹钟', no_events:'暂无事件',
    never:'永不', weekdays:'工作日', daily:'每天', weekend:'周末', custom:'自定义',
    edit:'改', del:'删', settings:'设置', settings_sub:'偏好保存在云端，并下发到设备',
    sec_loc:'位置与时间', city:'城市（决定天气）', tz:'时区', hourfmt:'时间格式',
    h24:'24 小时制', h12:'12 小时制', lang:'语言',
    sec_disp:'显示', unit:'温度单位', uc:'摄氏度 °C', uf:'华氏度 °F',
    wx:'天气刷新间隔', m10:'10 分钟', m30:'30 分钟', m60:'60 分钟', name:'设备名称',
    save:'保存并下发', saving:'保存中…',
    saved_push:(x)=>`已保存，下发到设备：${x}`, saved_same:'已保存（与设备一致，无需下发）',
    save_fail:(e)=>`保存失败：${e}`,
    dlg_new:'新增闹钟', dlg_edit:'编辑闹钟', time:'时间', repeat:'重复', label:'标签',
    ring:'铃声', snooze:'稍后提醒', snooze_len:'稍后提醒时长', delete_alarm:'删除闹钟',
    repeat_hint:'根据节假日安排设定闹钟，可设定为特定日期重复，也可以自定义闹钟。',
    minutes:(n)=>`${n} 分钟`, days:['一','二','三','四','五','六','日'],
    days_long:['周一','周二','周三','周四','周五','周六','周日'],
    day_range:(a,b)=>`${a}至${b}`,
    rings:{'故事时间':'故事时间','清晨':'清晨','鸟鸣':'鸟鸣','经典':'经典','渐强':'渐强'},
  },
  en: {
    title:'Alarm Clock · Remote', tab_home:'Home', tab_settings:'Settings', logout:'Sign out',
    connecting:'Connecting…',
    sec_net:'Network', ssid:'Wi-Fi SSID', wpass:'Wi-Fi password',
    wifi_save:'Push to device', need_ssid:'SSID is required',
    sent:'Pushed, the device is reconnecting…',
    auto_hint:'Changes are pushed immediately', pushall:'Push all settings again',
    wx_updated:'Weather updated', wx_never:'Weather not fetched yet', mins_ago:(m)=>`${m} min ago`,
    wifi_hint:'The device drops and reconnects, about 5–10 s. If it fails, fix it on the device\'s local page (the IP shown on screen).', online:(w)=>w ? `Online · ${w}` : 'Online', offline:'Offline (local alarms)',
    down:'Service unavailable', air:(n)=>`AQI ${n}`,
    lamp:'Light', lamp_hint:'Relay · bedroom ceiling light', ac:'AC', ac_hint:'IR · room air conditioner',
    on:'On', off:'Off', alarms:'Alarms', add:'+ Add', events:'Events',
    no_alarms:'No alarms yet', no_events:'No events yet',
    never:'Never', weekdays:'Weekdays', daily:'Every day', weekend:'Weekends', custom:'Custom',
    edit:'Edit', del:'Del', settings:'Settings', settings_sub:'Saved in the cloud and pushed to the device',
    sec_loc:'Location & time', city:'City (drives weather)', tz:'Time zone', hourfmt:'Time format',
    h24:'24-hour', h12:'12-hour', lang:'Language',
    sec_disp:'Display', unit:'Temperature unit', uc:'Celsius °C', uf:'Fahrenheit °F',
    wx:'Weather refresh', m10:'10 min', m30:'30 min', m60:'60 min', name:'Device name',
    save:'Save & push', saving:'Saving…',
    saved_push:(x)=>`Saved, pushed: ${x}`, saved_same:'Saved (already in sync)',
    save_fail:(e)=>`Save failed: ${e}`,
    dlg_new:'New alarm', dlg_edit:'Edit alarm', time:'Time', repeat:'Repeat', label:'Label',
    ring:'Sound', snooze:'Snooze', snooze_len:'Snooze duration', delete_alarm:'Delete alarm',
    repeat_hint:'Set alarms around public holidays, specific dates, or your own schedule.',
    minutes:(n)=>`${n} min`, days:['M','T','W','T','F','S','S'],
    days_long:['Mon','Tue','Wed','Thu','Fri','Sat','Sun'],
    day_range:(a,b)=>`${a}–${b}`,
    rings:{'故事时间':'Storytime','清晨':'Morning','鸟鸣':'Birdsong','经典':'Classic','渐强':'Crescendo'},
  },
};

const CITIES = [
  ['北京','Asia/Shanghai','Beijing'],['上海','Asia/Shanghai','Shanghai'],
  ['广州','Asia/Shanghai','Guangzhou'],['深圳','Asia/Shanghai','Shenzhen'],
  ['成都','Asia/Shanghai','Chengdu'],['重庆','Asia/Shanghai','Chongqing'],
  ['杭州','Asia/Shanghai','Hangzhou'],['南京','Asia/Shanghai','Nanjing'],
  ['武汉','Asia/Shanghai','Wuhan'],['西安','Asia/Shanghai',"Xi'an"],
  ['天津','Asia/Shanghai','Tianjin'],['苏州','Asia/Shanghai','Suzhou'],
  ['长沙','Asia/Shanghai','Changsha'],['郑州','Asia/Shanghai','Zhengzhou'],
  ['青岛','Asia/Shanghai','Qingdao'],['大连','Asia/Shanghai','Dalian'],
  ['厦门','Asia/Shanghai','Xiamen'],['昆明','Asia/Shanghai','Kunming'],
  ['哈尔滨','Asia/Shanghai','Harbin'],['乌鲁木齐','Asia/Shanghai','Urumqi'],
  ['香港','Asia/Hong_Kong','Hong Kong'],['台北','Asia/Taipei','Taipei'],
  ['东京','Asia/Tokyo','Tokyo'],['首尔','Asia/Seoul','Seoul'],
  ['新加坡','Asia/Singapore','Singapore'],['曼谷','Asia/Bangkok','Bangkok'],
  ['迪拜','Asia/Dubai','Dubai'],['伦敦','Europe/London','London'],
  ['巴黎','Europe/Paris','Paris'],['柏林','Europe/Berlin','Berlin'],
  ['莫斯科','Europe/Moscow','Moscow'],['纽约','America/New_York','New York'],
  ['芝加哥','America/Chicago','Chicago'],['洛杉矶','America/Los_Angeles','Los Angeles'],
  ['悉尼','Australia/Sydney','Sydney'],
];

const TZ_LABEL = {
  zh: {
    'Asia/Shanghai':'中国标准时间（UTC+8）', 'Asia/Hong_Kong':'香港时间（UTC+8）',
    'Asia/Taipei':'台北时间（UTC+8）', 'Asia/Tokyo':'日本标准时间（UTC+9）',
    'Asia/Seoul':'韩国标准时间（UTC+9）', 'Asia/Singapore':'新加坡时间（UTC+8）',
    'Asia/Bangkok':'曼谷时间（UTC+7）', 'Asia/Dubai':'迪拜时间（UTC+4）',
    'Asia/Kolkata':'印度标准时间（UTC+5:30）', 'Europe/London':'英国时间（UTC+0/+1）',
    'Europe/Paris':'中欧时间（UTC+1/+2）', 'Europe/Berlin':'中欧时间（UTC+1/+2）',
    'Europe/Moscow':'莫斯科时间（UTC+3）', 'America/New_York':'美国东部时间（UTC-5/-4）',
    'America/Chicago':'美国中部时间（UTC-6/-5）', 'America/Denver':'美国山地时间（UTC-7/-6）',
    'America/Los_Angeles':'美国太平洋时间（UTC-8/-7）', 'America/Sao_Paulo':'巴西时间（UTC-3）',
    'Australia/Sydney':'悉尼时间（UTC+10/+11）', 'Pacific/Auckland':'新西兰时间（UTC+12/+13）',
    'UTC':'协调世界时（UTC）',
  },
  en: {
    'Asia/Shanghai':'China Standard Time (UTC+8)', 'Asia/Hong_Kong':'Hong Kong Time (UTC+8)',
    'Asia/Taipei':'Taipei Time (UTC+8)', 'Asia/Tokyo':'Japan Standard Time (UTC+9)',
    'Asia/Seoul':'Korea Standard Time (UTC+9)', 'Asia/Singapore':'Singapore Time (UTC+8)',
    'Asia/Bangkok':'Indochina Time (UTC+7)', 'Asia/Dubai':'Gulf Time (UTC+4)',
    'Asia/Kolkata':'India Standard Time (UTC+5:30)', 'Europe/London':'UK Time (UTC+0/+1)',
    'Europe/Paris':'Central European Time (UTC+1/+2)', 'Europe/Berlin':'Central European Time (UTC+1/+2)',
    'Europe/Moscow':'Moscow Time (UTC+3)', 'America/New_York':'US Eastern Time (UTC-5/-4)',
    'America/Chicago':'US Central Time (UTC-6/-5)', 'America/Denver':'US Mountain Time (UTC-7/-6)',
    'America/Los_Angeles':'US Pacific Time (UTC-8/-7)', 'America/Sao_Paulo':'Brasília Time (UTC-3)',
    'Australia/Sydney':'Sydney Time (UTC+10/+11)', 'Pacific/Auckland':'New Zealand Time (UTC+12/+13)',
    'UTC':'Coordinated Universal Time',
  },
};
const TZS = Object.keys(TZ_LABEL.zh);

const COND_EN = {
  '晴':'Clear','多云':'Partly cloudy','阴':'Overcast','小雨':'Light rain','中雨':'Rain',
  '大雨':'Heavy rain','暴雨':'Storm','雷阵雨':'Thunderstorm','雷雨':'Thunderstorm',
  '小雪':'Light snow','中雪':'Snow','大雪':'Heavy snow','雾':'Fog','霾':'Haze',
  '沙尘':'Dust','浮尘':'Dust','阵雨':'Showers','阵雪':'Snow showers','冻雨':'Freezing rain',
  '少云':'Mostly clear',
};
const condText = s => lang === 'en' ? (COND_EN[s] || s || '—') : (s || '—');

const PRESETS = [[0,'never'],[31,'weekdays'],[127,'daily'],[96,'weekend']];
const RINGS = ['故事时间','清晨','鸟鸣','经典','渐强'];
const SNOOZE_MIN = [5,9,10,15,20];
const ITEM = 44;

let state = {}, settings = {}, editing = null, view = 'home';
let lang = localStorage.getItem('lang') || 'zh';
const t = k => I18N[lang][k];
const tf = (k, ...a) => { const v = I18N[lang][k]; return typeof v === 'function' ? v(...a) : v; };
const ringText = r => (I18N[lang].rings[r] || r);

function maskText(m) {
  const p = PRESETS.find(([v]) => v === m);
  if (p) return t(p[1]);
  if (!m) return t('never');
  const D = I18N[lang].days_long;
  const on = [...Array(7).keys()].filter(i => m & (1 << i));
  if (on.length >= 3) {
    const run = on.every((d, k) => k === 0 || d === on[k - 1] + 1);
    if (run) return I18N[lang].day_range(D[on[0]], D[on[on.length - 1]]);
  }
  return on.map(d => D[d]).join(lang === 'en' ? ', ' : '、');
}

async function api(path, opts = {}) {
  const r = await fetch(path, {headers: {'Content-Type': 'application/json'}, ...opts});
  if (r.status === 401) { location.replace('/login.html'); throw new Error('unauthorized'); }
  if (!r.ok) throw new Error(`${r.status} ${path}`);
  return r.json();
}

async function guard() {
  try {
    const r = await fetch('/api/me');
    if (!r.ok) { location.replace('/login.html'); return false; }
    return true;
  } catch { location.replace('/login.html'); return false; }
}

/* ---------------------------------------------------------------- i18n apply */
function applyLang() {
  document.documentElement.lang = lang === 'zh' ? 'zh-CN' : 'en';
  document.title = t('title');
  $$('[data-i18n]').forEach(el => {
    const k = el.dataset.i18n;
    if (typeof I18N[lang][k] === 'string') el.textContent = t(k);
  });
  buildSelects();
  paintClock();
  if (state.online !== undefined) paintState(state);
  if (editing) { $('#shTitle').textContent = editing.id ? t('dlg_edit') : t('dlg_new');
                 $('#vRepeat').textContent = maskText(editing.mask);
                 $('#vRing').textContent = ringText(editing.ring);
                 $('#vSnoozeMin').textContent = tf('minutes', editing.snooze_min); }
}

function buildSelects() {
  const city = $('#sCity').value || settings.city || '成都';
  const tzv = $('#sTz').value || settings.tz || 'Asia/Shanghai';
  $('#sCity').innerHTML = CITIES
    .map(([zh, , en]) => `<option value="${zh}">${lang === 'en' ? en : zh}</option>`).join('');
  $('#sTz').innerHTML = TZS
    .map(z => `<option value="${z}">${(TZ_LABEL[lang][z] || z)}</option>`).join('');
  $('#sCity').value = city;
  $('#sTz').value = tzv;
}

/* ---------------------------------------------------------------- routing */
function route() {
  view = location.hash === '#/settings' ? 'settings' : 'home';
  $('#view-home').hidden = view !== 'home';
  $('#view-settings').hidden = view !== 'settings';
  $$('.tab').forEach(el => el.classList.toggle('active', el.dataset.view === view));
  if (view === 'settings') loadSettings();
}
window.addEventListener('hashchange', route);

/* ---------------------------------------------------------------- clock */
function paintClock() {
  const now = new Date();
  const p = n => String(n).padStart(2, '0');
  let h = now.getHours(), ap = '';
  if (settings.hour12) { ap = h < 12 ? 'AM' : 'PM'; h = h % 12 || 12; }
  $('#clock').textContent = `${settings.hour12 ? h : p(h)}:${p(now.getMinutes())}:${p(now.getSeconds())}`;
  $('#ampm').textContent = ap;
}
setInterval(paintClock, 1000);

/* ---------------------------------------------------------------- home */
function paintState(s) {
  state = s;
  $('#dot').className = 'dot' + (s.online ? ' on' : '');
  $('#conn').textContent = s.online ? tf('online', s.net?.ssid || '') : t('offline');
  const cs = $('#curSsid');
  if (cs) cs.textContent = (lang === 'en' ? 'Current: ' : '当前：') + (s.net?.ssid || '—');
  const f = !!settings.fahrenheit;
  const v = s.env?.t;
  $('#temp').textContent = v == null ? '--' : (f ? Math.round(v * 9 / 5 + 32) : v);
  $('#unit').textContent = f ? '°F' : '°C';
  $('#cond').textContent = condText(s.cond || s.env?.cond);
  $('#place').textContent = `${s.env?.city || settings.city || '—'} · ${tf('air', s.env?.aqi ?? '--')}`;
  paintWxTime(s.env?.ts);
  const lampOn = !!s.light?.on;
  $('#lamp').className = 'toggle ' + (lampOn ? 'on' : 'off');
  $('#lamp').textContent = lampOn ? t('on') : t('off');
  const acOn = !!s.ac?.on;
  $('#ac').className = 'toggle ' + (acOn ? 'on' : 'off');
  $('#ac').textContent = acOn ? t('on') : t('off');
  $('#actemp').textContent = `${s.ac?.temp ?? 26} °C`;
}

function paintAlarms(rows) {
  const ul = $('#alarms');
  ul.innerHTML = '';
  if (!rows.length) { ul.innerHTML = `<li class="muted">${t('no_alarms')}</li>`; return; }
  for (const a of rows) {
    const li = document.createElement('li');
    li.className = 'arow';
    const hh = settings.hour12 ? (a.h % 12 || 12) : String(a.h).padStart(2, '0');
    const main = document.createElement('button');
    main.className = 'amain';
    main.innerHTML = `<span class="atime">${hh}:${String(a.m).padStart(2, '0')}${
      settings.hour12 ? `<small>${a.h < 12 ? 'AM' : 'PM'}</small>` : ''}</span>
      <span class="arep">${a.label || ''}<em>${maskText(a.mask)}</em></span>`;
    main.onclick = () => openSheet(a);
    const sw = document.createElement('label');
    sw.className = 'switch';
    sw.innerHTML = `<input type="checkbox" ${a.on ? 'checked' : ''}><span></span>`;
    sw.querySelector('input').onchange = async e => {
      await api(`/api/devices/${DEV}/alarms/${a.id}`, {method: 'PUT', body: JSON.stringify(
        {...a, on: e.target.checked})});
      refresh();
    };
    li.append(main, sw);
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
    $('#events').innerHTML = ev.length
      ? ev.map(e => `<li>${new Date(e.ts * 1000).toLocaleTimeString()} · ${e.type}</li>`).join('')
      : `<li>${t('no_events')}</li>`;
  } catch { $('#conn').textContent = t('down'); }
}

/* weather timestamp from the device */
function paintWxTime(ts) {
  const el = $('#wxTime'); if (!el) return;
  if (!ts) { el.textContent = t('wx_never'); return; }
  const d = new Date(ts * 1000);
  const p = n => String(n).padStart(2, '0');
  const stamp = `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ` +
                `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
  const mins = Math.max(0, Math.round((Date.now() - d.getTime()) / 60000));
  el.textContent = `${t('wx_updated')}: ${stamp} · ${tf('mins_ago', mins)}`;
}

/* ---------------------------------------------------------------- settings */
async function loadSettings() {
  try { settings = await api(`/api/devices/${DEV}/settings`); } catch { settings = {}; }
  if (settings.lang === 'zh' || settings.lang === 'en') {
    lang = settings.lang; localStorage.setItem('lang', lang); applyLang();
  }
  buildSelects();
  $('#sCity').value = settings.city ?? '成都';
  $('#sTz').value = settings.tz ?? 'Asia/Shanghai';
  $('#sHour').value = settings.hour12 ? '12' : '24';
  $('#sUnit').value = settings.fahrenheit ? 'f' : 'c';
  $('#sWx').value = String(settings.wx_interval ?? 30);
  $('#sLang').value = lang;
  $('#sName').value = settings.name ?? $('#devName').textContent;
}

$('#sCity').addEventListener('change', () => {
  const hit = CITIES.find(([c]) => c === $('#sCity').value);
  if (hit) $('#sTz').value = hit[1];
});

$('#sLang').addEventListener('change', () => {
  lang = $('#sLang').value; localStorage.setItem('lang', lang); applyLang(); refresh();
});

let saveTimer = 0;
function scheduleSave() {
  clearTimeout(saveTimer);
  saveTimer = setTimeout(saveSettings, 350);
}

async function saveSettings() {
  const body = {
    city: $('#sCity').value, tz: $('#sTz').value,
    hour12: $('#sHour').value === '12', fahrenheit: $('#sUnit').value === 'f',
    wx_interval: Number($('#sWx').value),
    name: $('#sName').value.trim() || '卧室闹钟', lang,
  };
  $('#saveHint').textContent = t('saving');
  try {
    const r = await api(`/api/devices/${DEV}/settings`, {method: 'PUT', body: JSON.stringify(body)});
    settings = r.settings;
    $('#devName').textContent = r.settings.name;
    const pushed = Object.keys(r.pushed || {});
    $('#saveHint').textContent = pushed.length
      ? tf('saved_push', pushed.join(lang === 'en' ? ', ' : '、')) : t('saved_same');
    paintClock(); refresh();
  } catch (e) { $('#saveHint').textContent = tf('save_fail', e.message); }
}

$('#saveSettings').onclick = saveSettings;
['#sCity', '#sTz', '#sHour', '#sUnit', '#sWx', '#sLang'].forEach(sel => {
  $(sel).addEventListener('change', scheduleSave);
});
$('#sName').addEventListener('change', scheduleSave);
$('#sName').addEventListener('blur', scheduleSave);

/* ---------------------------------------------------------------- wheel */
function buildWheel(el, count, init) {
  el.innerHTML = '';
  for (let i = 0; i < count; i++) {
    const d = document.createElement('div');
    d.className = 'witem';
    d.textContent = String(i).padStart(2, '0');
    el.append(d);
  }
  el.scrollTop = init * ITEM;
  if (!el._bound) {
    el._bound = true;
    el.addEventListener('scroll', () => {
      clearTimeout(el._t);
      el._t = setTimeout(() => settleWheel(el), 110);
    });
  }
  requestAnimationFrame(() => settleWheel(el));
}

function settleWheel(el) {
  const max = el.children.length - 1;
  let idx = Math.round(el.scrollTop / ITEM);
  idx = Math.max(0, Math.min(max, idx));
  el.scrollTop = idx * ITEM;
  el._idx = idx;
  [...el.children].forEach((c, i) => {
    const d = Math.abs(i - idx);
    c.classList.toggle('on', d === 0);
    c.style.opacity = d === 0 ? 1 : d === 1 ? 0.55 : 0.25;
  });
}
const wheelValue = el => Math.max(0, Math.min(el.children.length - 1, Math.round(el.scrollTop / ITEM)));

/* ---------------------------------------------------------------- sheet */
function openSheet(a) {
  editing = a ? {...a} : {h:7, m:30, mask:31, label:'', ring:'故事时间', snooze:true, snooze_min:9, on:true};
  $('#shTitle').textContent = a ? t('dlg_edit') : t('dlg_new');
  $('#vLabel').value = editing.label || '';
  $('#vLabel').placeholder = lang === 'en' ? 'Alarm' : '闹钟';
  $('#vRing').textContent = ringText(editing.ring);
  $('#vSnooze').checked = !!editing.snooze;
  $('#vSnoozeMin').textContent = tf('minutes', editing.snooze_min);
  $('#vRepeat').textContent = maskText(editing.mask);
  $('#shDelete').hidden = !editing.id;
  $('#sheet').hidden = false;
  document.body.style.overflow = 'hidden';
  buildWheel($('#wH'), 24, editing.h);
  buildWheel($('#wM'), 60, editing.m);
}

function closeSheet() {
  $('#sheet').hidden = true;
  document.body.style.overflow = '';
  editing = null;
}

$('#shCancel').onclick = closeSheet;
$('#shSave').onclick = async () => {
  const body = {
    h: wheelValue($('#wH')), m: wheelValue($('#wM')),
    mask: editing.mask, on: editing.on !== false,
    label: $('#vLabel').value.slice(0, 9),
    ring: editing.ring, snooze: $('#vSnooze').checked,
    snooze_min: editing.snooze_min,
  };
  if (editing.id) await api(`/api/devices/${DEV}/alarms/${editing.id}`, {method: 'PUT', body: JSON.stringify(body)});
  else await api(`/api/devices/${DEV}/alarms`, {method: 'POST', body: JSON.stringify(body)});
  closeSheet(); refresh();
};
$('#shDelete').onclick = async () => {
  if (!editing?.id) return;
  await api(`/api/devices/${DEV}/alarms/${editing.id}`, {method: 'DELETE'});
  closeSheet(); refresh();
};
$('#addBtn').onclick = () => openSheet(null);

/* ---------------------------------------------------------------- pickers */
function openPicker(title) {
  $('#pkTitle').textContent = title;
  $('#picker').hidden = false;
}
function closePicker() { $('#picker').hidden = true; }
$('#pkClose').onclick = closePicker;

$('#rRepeat').onclick = () => {
  const body = $('#pkBody');
  body.innerHTML = '';
  for (const [mask, key] of PRESETS) {
    const b = document.createElement('button');
    b.className = 'row tap';
    b.innerHTML = `<span>${t(key)}</span><b>${mask === editing.mask ? '✓' : ''}</b>`;
    b.onclick = () => { editing.mask = mask; syncRepeat(); };
    body.append(b);
  }
  const days = document.createElement('div');
  days.className = 'days';
  I18N[lang].days.forEach((d, i) => {
    const c = document.createElement('button');
    c.className = 'day' + (editing.mask & (1 << i) ? ' on' : '');
    c.textContent = d;
    c.onclick = () => {
      editing.mask ^= (1 << i);
      c.classList.toggle('on');
      $('#vRepeat').textContent = maskText(editing.mask);
      $$('#pkBody .row b').forEach((b, k) => {
        b.textContent = PRESETS[k][0] === editing.mask ? '✓' : '';
      });
    };
    days.append(c);
  });
  body.append(days);
  const hint = document.createElement('p');
  hint.className = 'hint';
  hint.textContent = t('repeat_hint');
  body.append(hint);
  openPicker(t('repeat'));
};

$('#rRing').onclick = () => {
  const body = $('#pkBody');
  body.innerHTML = '';
  for (const r of RINGS) {
    const b = document.createElement('button');
    b.className = 'row tap';
    b.innerHTML = `<span>${ringText(r)}</span><b>${r === editing.ring ? '✓' : ''}</b>`;
    b.onclick = () => { editing.ring = r; $('#vRing').textContent = ringText(r); closePicker(); };
    body.append(b);
  }
  openPicker(t('ring'));
};

$('#rSnoozeMin').onclick = () => {
  const body = $('#pkBody');
  body.innerHTML = '';
  for (const n of SNOOZE_MIN) {
    const b = document.createElement('button');
    b.className = 'row tap';
    b.innerHTML = `<span>${tf('minutes', n)}</span><b>${n === editing.snooze_min ? '✓' : ''}</b>`;
    b.onclick = () => { editing.snooze_min = n; $('#vSnoozeMin').textContent = tf('minutes', n); closePicker(); };
    body.append(b);
  }
  openPicker(t('snooze_len'));
};

function syncRepeat() {
  $('#vRepeat').textContent = maskText(editing.mask);
  closePicker();
}

/* ---------------------------------------------------------------- devices */
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

$('#wifiBtn')?.addEventListener('click', async () => {
  const ssid = $('#sSsid').value.trim(), pass = $('#sWpass').value;
  const msg = $('#wifiMsg');
  if (!ssid) { msg.textContent = t('need_ssid'); return; }
  msg.textContent = '…';
  try {
    await api(`/api/devices/${DEV}/cmd`, {method: 'POST',
      body: JSON.stringify({op: 'wifi.set', ssid, pass})});
    msg.textContent = t('sent');
    $('#sSsid').value = ''; $('#sWpass').value = '';
  } catch (e) { msg.textContent = String(e); }
});

$('#logout')?.addEventListener('click', async () => {
  try { await fetch('/api/logout', {method: 'POST'}); } catch {}
  location.replace('/login.html');
});

/* ---------------------------------------------------------------- boot */
(async function boot() {
  if (!await guard()) return;
  try {
    settings = await api(`/api/devices/${DEV}/settings`);
    if (settings.lang === 'zh' || settings.lang === 'en') {
      lang = settings.lang; localStorage.setItem('lang', lang);
    }
  } catch {}
  applyLang();
  route();
  paintClock();
  refresh();
  setInterval(refresh, 2000);
})();
