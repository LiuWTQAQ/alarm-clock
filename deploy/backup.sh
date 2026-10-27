#!/usr/bin/env bash
# 每日备份 SQLite（用 SQLite 在线备份 API，服务不用停）。
# 保留最近 14 份，旧的自动删。
set -euo pipefail

ROOT=${ALARM_ROOT:-/opt/alarm}
DB=${ALARM_DB:-$ROOT/data/app.db}
OUT=${ALARM_BACKUP_DIR:-$ROOT/backups}
KEEP=${ALARM_BACKUP_KEEP:-14}

mkdir -p "$OUT"
[ -f "$DB" ] || { echo "no database at $DB"; exit 0; }

STAMP=$(date +%Y%m%d-%H%M%S)
DEST="$OUT/app-$STAMP.db"

python3 - "$DB" "$DEST" <<'PY'
import sqlite3, sys
src, dst = sys.argv[1], sys.argv[2]
a = sqlite3.connect(src)
b = sqlite3.connect(dst)
with b:
    a.backup(b)
b.close(); a.close()
PY

gzip -f "$DEST"
chown alarm:alarm "$DEST.gz" 2>/dev/null || true

# 只留最近 N 份
ls -1t "$OUT"/app-*.db.gz 2>/dev/null | tail -n +$((KEEP + 1)) | xargs -r rm -f

echo "$(date '+%F %T')  backup ok: $DEST.gz ($(du -h "$DEST.gz" | cut -f1))"
