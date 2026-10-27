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
chown "$MOSQ_UID:$MOSQ_UID" /etc/mosquitto/passwd 2>/dev/null || true
chmod 600 /etc/mosquitto/passwd

echo "==> 3/6 写入独立配置 /etc/mosquitto/alarm.conf"
cat >/etc/mosquitto/alarm.conf <<EOF
# 由 deploy/install_mosquitto.sh 生成，避免与发行版自带配置冲突
per_listener_settings false
listener 1883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/passwd
max_keepalive 120
persistence false
log_dest journal
EOF

echo "==> 4/6 让 systemd 使用这份配置"
mkdir -p /etc/systemd/system/mosquitto.service.d
cat >/etc/systemd/system/mosquitto.service.d/alarm.conf <<EOF
[Service]
ExecStart=
ExecStart=/usr/sbin/mosquitto -c /etc/mosquitto/alarm.conf
EOF
systemctl daemon-reload
systemctl enable mosquitto >/dev/null 2>&1 || true
systemctl restart mosquitto
sleep 1

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
