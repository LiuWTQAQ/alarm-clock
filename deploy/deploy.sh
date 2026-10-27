#!/usr/bin/env bash
# 把当前仓库内容发布到 /opt/alarm/releases/<时间戳>，再用软链切换，失败可秒回滚。
set -euo pipefail

REPO="${REPO:-$HOME/alarm-clock}"
ROOT=/opt/alarm
STAMP=$(date +%Y%m%d-%H%M%S)
REL="$ROOT/releases/$STAMP"

echo "==> 发布 $STAMP"
mkdir -p "$REL" "$ROOT/data"

copy_tree() {
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --delete \
      --exclude .git --exclude .pio --exclude firmware/.pio --exclude node_modules \
      --exclude '*.db' "$REPO/" "$REL/"
  else
    echo "    (no rsync, using cp)"
    ( cd "$REPO" && tar -cf - --exclude .git --exclude .pio --exclude node_modules \
        --exclude '*.db' . ) | ( cd "$REL" && tar -xf - )
  fi
}
copy_tree
touch "$REL"   # rsync -a 会把源目录的 mtime 复制过来，这里改回当前时间

if [ ! -x "$ROOT/venv/bin/uvicorn" ]; then
  echo "==> 首次创建虚拟环境"
  python3 -m venv "$ROOT/venv"
  "$ROOT/venv/bin/pip" install -q --upgrade pip
fi
# 每次发布都对齐依赖（已装好的会秒过）
"$ROOT/venv/bin/pip" install -q -r "$REL/server/requirements.txt"

# 数据文件放在 releases 之外，发布不会覆盖
mkdir -p "$ROOT/data"
rm -f "$REL/server/data.db"
ln -sfn "$ROOT/data/app.db" "$REL/server/data.db" 2>/dev/null || true

if [ ! -f "$REL/web/index.html" ]; then
  echo "!!  release 里没有 web/index.html，拒绝切换（检查 $REPO/web）"
  exit 1
fi

ln -sfn "$REL" "$ROOT/current.new" && mv -Tf "$ROOT/current.new" "$ROOT/current"
chown -R alarm:alarm "$ROOT" 2>/dev/null || true

echo "==> 重启服务"
systemctl restart alarm

# /api/health 是公开接口；/api/devices 需要登录，会返回 401，不能用来判活。
ok=0
for _ in $(seq 1 24); do
  code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 http://127.0.0.1:8001/api/health || true)
  [ "$code" = "200" ] && { ok=1; break; }
  sleep 0.5
done

if [ "$ok" = "1" ] && systemctl is-active --quiet alarm; then
  echo "==> OK  https://alarm.yoshino-liuwt.top"
  # 按目录名（YYYYmmdd-HHMMSS）排序，不要用 mtime 排序：
  # rsync -a 会把源目录的 mtime 复制到 release 根目录，用 mtime 会把刚发布的版本当旧版本删掉
  CUR=$(readlink -f "$ROOT/current")
  for d in $(ls -1d "$ROOT"/releases/* | sort -r | tail -n +4); do
    [ "$d" = "$CUR" ] && continue
    rm -rf "$d"
  done
else
  echo "!!  健康检查失败（http=$code, unit=$(systemctl is-active alarm 2>/dev/null)），回滚"
  echo "!!  健康检查失败，回滚到上一个版本"
  PREV=$(ls -1d "$ROOT"/releases/* | sort -r | sed -n 2p)
  [ -n "$PREV" ] && ln -sfn "$PREV" "$ROOT/current"
  systemctl restart alarm
  exit 1
fi
