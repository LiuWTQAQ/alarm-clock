#!/usr/bin/env bash
# 在腾讯云服务器上安装并配置 Mosquitto（0.0.0.0:1883 + 账号密码）
# 用法：MQ_PASS='YourPassword' bash deploy/install_mosquitto.sh
set -euo pipefail

MQ_USER="${MQ_USER:-alarm}"
MQ_PASS="${MQ_PASS:?请先设置 MQ_PASS，例如：MQ_PASS='xxxx' bash deploy/install_mosquitto.sh}"

echo "==> 1/6 安装 mosquitto"
if ! command -v mosquitto >/dev/null 2>&1; then
  if command -v dnf >/dev/null 2>&1; then
    dnf install -y mosquitto >/dev/null || { echo "!! dnf 安装失败，先执行 dnf makecache 再试"; exit 1; }
  else
    yum install -y mosquitto >/dev/null || { echo "!! yum 安装失败，先执行 yum makecache 再试"; exit 1; }
  fi
fi
command -v mosquitto >/dev/null 2>&1 || { echo "!! 安装后仍找不到 mosquitto"; exit 1; }

echo "==> 2/6 生成密码文件"
MOSQ_UID=$(id -u mosquitto 2>/dev/null || echo 0)
mosquitto_passwd -b -c /etc/mosquitto/passwd "$MQ_USER" "$MQ_PASS"
# 640 root:mosquitto：只有 root 和 broker 组能读（文件里是 PBKDF2 哈希）
chown root:mosquitto /etc/mosquitto/passwd
chmod 640 /etc/mosquitto/passwd
command -v restorecon >/dev/null 2>&1 && restorecon -F /etc/mosquitto/passwd 2>/dev/null || true

echo "==> 3/6 定位发行版配置文件"

UNIT_CONF=$(systemctl cat mosquitto 2>/dev/null \
  | sed -n 's/.*-c \([^ ]*\).*/\1/p' | head -1)
UNIT_CONF="${UNIT_CONF:-/etc/mosquitto/mosquitto.conf}"
echo "    使用配置文件：$UNIT_CONF"
if [ -f "$UNIT_CONF" ] && [ ! -f "$UNIT_CONF.orig" ]; then
  cp -a "$UNIT_CONF" "$UNIT_CONF.orig"
  echo "    已备份原配置到 $UNIT_CONF.orig"
fi

echo "==> 4/6 写入配置并重启"
cat >"$UNIT_CONF" <<EOF
# 由 deploy/install_mosquitto.sh 生成
# root 启动后降权到 mosquitto，密码文件 640 root:mosquitto 才能读到
user mosquitto
per_listener_settings false
listener 1883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/passwd
max_keepalive 120
persistence false
log_dest stderr
connection_messages true
EOF

# 清掉上一版脚本留下的 drop-in，避免重复/冲突
rm -f /etc/systemd/system/mosquitto.service.d/alarm.conf
rmdir /etc/systemd/system/mosquitto.service.d 2>/dev/null || true
systemctl daemon-reload
systemctl enable mosquitto >/dev/null 2>&1 || true
systemctl restart mosquitto
sleep 1

chown root:mosquitto /etc/mosquitto/passwd 2>/dev/null || true
chmod 640 /etc/mosquitto/passwd 2>/dev/null || true

echo "==> 5/6 放行本机防火墙（腾讯云控制台仍需手动放行 1883）"
if command -v firewall-cmd >/dev/null 2>&1 && firewall-cmd --state >/dev/null 2>&1; then
  firewall-cmd --permanent --add-port=1883/tcp >/dev/null
  firewall-cmd --reload >/dev/null
  echo "    firewalld 已放行 1883/tcp"
else
  echo "    firewalld 未启用，跳过"
fi

echo "==> 6/6 自检"
if ! systemctl is-active --quiet mosquitto; then
  echo "!! mosquitto 没起来，最近日志："
  journalctl -u mosquitto -n 20 --no-pager || true
  exit 1
fi
if command -v ss >/dev/null 2>&1 && ! ss -lnt | grep -q ':1883 '; then
  echo "!! 1883 没在监听，最近日志："
  journalctl -u mosquitto -n 20 --no-pager || true
  exit 1
fi
echo "    1883 正在监听"

if command -v mosquitto_pub >/dev/null 2>&1; then
  if timeout 5 mosquitto_pub -h 127.0.0.1 -p 1883 -u "$MQ_USER" -P "$MQ_PASS" \
        -t 'alarm/selfcheck' -m ping -q 1; then
    echo "    账号密码验证通过"
  else
    echo "!! 用 $MQ_USER 发布失败，检查密码文件与 allow_anonymous"
    exit 1
  fi
fi

echo
echo "完成。接下来在 /etc/systemd/system/alarm.service 里设置："
echo "  Environment=MQTT_URL=mqtt://${MQ_USER}:<同样的密码>@127.0.0.1:1883"
echo "然后 systemctl daemon-reload && systemctl restart alarm"
echo
echo "别忘了在腾讯云控制台 → 防火墙，放行 TCP 1883。"
