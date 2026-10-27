#!/usr/bin/env bash
# 一次性把运维三件事配好：MQTT 密码文件权限、每日备份、日志轮转。
# 可重复执行。
set -euo pipefail
cd "$(dirname "$0")/.."
REPO=$(pwd)

say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

say "1/3 MQTT 密码文件权限"
if [ -f /etc/mosquitto/passwd ]; then
  chown root:mosquitto /etc/mosquitto/passwd
  chmod 640 /etc/mosquitto/passwd
  ls -l /etc/mosquitto/passwd
  if ! grep -q '^user mosquitto' /etc/mosquitto/mosquitto.conf 2>/dev/null; then
    echo "    !! mosquitto.conf 里没有 'user mosquitto'，正在补上"
    printf '\n# root 启动后降权\nuser mosquitto\n' >> /etc/mosquitto/mosquitto.conf
    systemctl restart mosquitto
  fi
  systemctl is-active --quiet mosquitto && echo "    mosquitto 运行中"
else
  echo "    没有 /etc/mosquitto/passwd，跳过（先跑 install_mosquitto.sh）"
fi

say "2/3 每日备份（03:30，保留 14 份）"
install -m 644 "$REPO/deploy/alarm-backup.service" /etc/systemd/system/alarm-backup.service
install -m 644 "$REPO/deploy/alarm-backup.timer"   /etc/systemd/system/alarm-backup.timer
systemctl daemon-reload
systemctl enable --now alarm-backup.timer >/dev/null
mkdir -p /opt/alarm/backups
chown -R alarm:alarm /opt/alarm/backups 2>/dev/null || true
systemctl start alarm-backup.service
ls -lh /opt/alarm/backups | tail -3

say "3/3 日志轮转（journald 200 MB / 30 天）"
install -d /etc/systemd/journald.conf.d
install -m 644 "$REPO/deploy/journald-alarm.conf" /etc/systemd/journald.conf.d/alarm.conf
systemctl restart systemd-journald
journalctl --disk-usage

echo
echo "完成。查看备份定时器：systemctl list-timers alarm-backup.timer --no-pager"
