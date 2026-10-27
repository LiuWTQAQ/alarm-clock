#!/usr/bin/env bash
# 在腾讯云服务器上安装并配置 Mosquitto（仅本机监听 1883 + 账号密码）
# 用法：MQ_PASS='YourPassword' bash deploy/install_mosquitto.sh
set -euo pipefail

MQ_USER="${MQ_USER:-alarm}"
MQ_PASS="${MQ_PASS:?请先设置 MQ_PASS，例如：MQ_PASS='xxxx' bash deploy/install_mosquitto.sh}"

echo "==> 安装 mosquitto"
if command -v dnf >/dev/null 2>&1; then
  dnf install -y mosquitto >/dev/null
else
  yum install -y mosquitto >/dev/null
fi

echo "==> 生成密码文件"
mosquitto_passwd -b -c /etc/mosquitto/passwd "$MQ_USER" "$MQ_PASS"
chown mosquitto:mosquitto /etc/mosquitto/passwd 2>/dev/null || true
chmod 600 /etc/mosquitto/passwd

echo "==> 写入配置 /etc/mosquitto/conf.d/alarm.conf"
cat >/etc/mosquitto/conf.d/alarm.conf <<EOF
# 由 deploy/install_mosquitto.sh 生成
listener 1883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/passwd
max_keepalive 120
persistence false
EOF

echo "==> 启动"
systemctl enable mosquitto >/dev/null 2>&1 || true
systemctl restart mosquitto
sleep 1

echo "==> 放行本机防火墙（腾讯云控制台仍需手动放行 1883）"
if command -v firewall-cmd >/dev/null 2>&1 && firewall-cmd --state >/dev/null 2>&1; then
  firewall-cmd --permanent --add-port=1883/tcp >/dev/null
  firewall-cmd --reload >/dev/null
  echo "    firewalld 已放行 1883/tcp"
fi

echo "==> 自检"
ss -lntp | grep -q ':1883' && echo "    1883 正在监听" || { echo "    !! 1883 未监听"; exit 1; }
mosquitto_sub -h 127.0.0.1 -u "$MQ_USER" -P "$MQ_PASS" -t 'alarm/#' -C 1 -W 1 >/dev/null 2>&1 \
  && echo "    账号密码可用" || echo "    (订阅测试超时，正常——当前没有设备在发消息)"

echo
echo "完成。接下来在 /etc/systemd/system/alarm.service 里设置："
echo "  Environment=MQTT_URL=mqtt://${MQ_USER}:<同样的密码>@127.0.0.1:1883"
echo "然后 systemctl daemon-reload && systemctl restart alarm"
echo
echo "别忘了在腾讯云控制台 → 防火墙，放行 TCP 1883。"
