#!/usr/bin/env bash
# 一条命令看清云端链路状态：服务、HTTP、MQTT、账号、设备在线。
# 用法：bash deploy/selfcheck.sh
set -uo pipefail

ROOT="${ROOT:-/opt/alarm}"
UNIT="${UNIT:-alarm}"
API="${API:-http://127.0.0.1:8001}"
ok=0; bad=0
pass() { printf '  \033[32m✔\033[0m %s\n' "$1"; ok=$((ok+1)); }
fail() { printf '  \033[31m✘\033[0m %s\n' "$1"; bad=$((bad+1)); }
warn() { printf '  \033[33m!\033[0m %s\n' "$1"; }

HAVE_SYSTEMD=1
command -v systemctl >/dev/null 2>&1 || HAVE_SYSTEMD=0

echo "== 1. systemd 服务"
if [ "$HAVE_SYSTEMD" = "0" ]; then
  warn "非 systemd 环境，跳过服务检查"
elif systemctl is-active --quiet "$UNIT"; then
  pass "alarm 服务运行中（$(systemctl show -p ActiveEnterTimestamp --value "$UNIT") 启动）"
else
  fail "alarm 服务未运行：journalctl -u $UNIT -n 50 --no-pager"
fi
[ "$HAVE_SYSTEMD" = "1" ] && { systemctl is-enabled --quiet "$UNIT" && pass "开机自启已启用" || warn "未设置开机自启：systemctl enable $UNIT"; }

echo "== 2. HTTP 接口"
code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 3 "$API/api/health" || true)
[ "$code" = "200" ] && pass "/api/health 200" || fail "/api/health 返回 $code（服务没起来？看 journalctl）"
code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 3 "$API/api/devices" || true)
[ "$code" = "401" ] && pass "未登录访问 /api/devices 返回 401（鉴权生效）" \
  || warn "未登录访问 /api/devices 返回 $code（期望 401）"

echo "== 3. MQTT 配置"
ENVLINE=""
[ "$HAVE_SYSTEMD" = "1" ] && ENVLINE=$(systemctl show -p Environment --value "$UNIT" 2>/dev/null || true)
MQTT_URL=$(printf '%s\n' "$ENVLINE" | tr ' ' '\n' | grep '^MQTT_URL=' | head -1 | cut -d= -f2-)
if [ -n "${MQTT_URL:-}" ]; then
  pass "MQTT_URL 已配置：$(printf '%s' "$MQTT_URL" | sed 's#//[^@]*@#//***:***@#')"
else
  warn "MQTT_URL 未配置——设备命令会留在进程内队列，真机收不到"
fi

echo "== 4. Mosquitto"
if [ "$HAVE_SYSTEMD" = "0" ]; then
  warn "非 systemd 环境，只检查端口"
elif systemctl is-active --quiet mosquitto; then
  pass "mosquitto 运行中"
else
  fail "mosquitto 未运行：systemctl status mosquitto"
fi
port_open() {
  if command -v ss >/dev/null 2>&1; then ss -lnt 2>/dev/null | grep -q ':1883 '
  elif command -v lsof >/dev/null 2>&1; then lsof -iTCP:1883 -sTCP:LISTEN >/dev/null 2>&1
  else nc -z 127.0.0.1 1883 >/dev/null 2>&1; fi
}
if port_open; then
  pass "1883 端口在监听"
else
  fail "1883 未监听（没装或没启动）"
fi

echo "== 5. 账号与连通性"
if command -v mosquitto_sub >/dev/null 2>&1 && [ -n "${MQTT_URL:-}" ]; then
  read -r HOST PORT MQUSER MQPASS < <(printf '%s' "$MQTT_URL" | python3 -c '
import sys, urllib.parse as u
p = u.urlparse(sys.stdin.read().strip())
print(p.hostname or "127.0.0.1", p.port or 1883, p.username or "", p.password or "")
')
  AUTH=()
  [ -n "$MQUSER" ] && AUTH+=(-u "$MQUSER")
  [ -n "$MQPASS" ] && AUTH+=(-P "$MQPASS")
  if timeout 6 mosquitto_pub -h "$HOST" -p "$PORT" "${AUTH[@]}" \
        -t 'alarm/selfcheck' -m 'ping' -q 1 2>/dev/null; then
    pass "用 ${MQUSER:-匿名} 连 $HOST:$PORT 发布成功"
  else
    fail "连 $HOST:$PORT 失败（密码错？控制台没放行 1883？）"
  fi
else
  warn "跳过（没装 mosquitto 客户端，或没配 MQTT_URL）"
fi

echo "== 6. 设备状态（直接读 SQLite）"
DB="$ROOT/data/app.db"
[ -f "$DB" ] || DB="$ROOT/server/data.db"
if [ -f "$DB" ]; then
  python3 - "$DB" <<'PY'
import sqlite3, sys, time
con = sqlite3.connect(sys.argv[1]); con.row_factory = sqlite3.Row
rows = con.execute("SELECT id,name,online,last_seen,rev FROM devices").fetchall()
if not rows:
    print("  ! 还没有设备记录（设备从没连过云端）")
for r in rows:
    state = "在线" if r["online"] else "离线"
    ls = r["last_seen"] or 0
    if not ls:
        ago = "从未上报"
    else:
        d = max(0, int(time.time()) - ls)
        ago = (f"{d}s 前" if d < 120 else
               f"{d//60} 分钟前" if d < 7200 else
               f"{d//3600} 小时前" if d < 86400 else f"{d//86400} 天前")
    print(f"  · {r['id']} {r['name'] or ''}   {state}   最后上报 {ago}   rev {r['rev']}")
PY
else
  warn "找不到 $DB"
fi

echo
if [ "$bad" -eq 0 ]; then
  printf '\033[32m全部通过（%d 项）\033[0m\n' "$ok"
else
  printf '\033[31m%d 项失败 / %d 项通过\033[0m\n' "$bad" "$ok"
  exit 1
fi
