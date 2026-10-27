#!/bin/bash
# 每天 21:00 自动快照 /Users/liuwentao/alarm-clock 并推送到 GitHub
# 由 launchd (com.liuwentao.alarmclock.daily) 调用，也可手动执行：
#   bash /Users/liuwentao/alarm-clock/scripts/daily_commit.sh
#
# 可通过环境变量覆盖：
#   ALARM_REPO=<仓库路径>  ALARM_LOG=<日志路径>

set -u

REPO="${ALARM_REPO:-/Users/liuwentao/alarm-clock}"
LOG="${ALARM_LOG:-$HOME/Library/Logs/alarm-clock-daily.log}"

mkdir -p "$(dirname "$LOG")"
exec >>"$LOG" 2>&1

echo "===== $(date '+%Y-%m-%d %H:%M:%S') ====="

cd "$REPO" || { echo "ERROR: repo not found: $REPO"; exit 1; }

BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null)"
[ -z "$BRANCH" ] && { echo "ERROR: not a git repo"; exit 1; }

# 没有任何改动（含未跟踪文件）就直接退出
if [ -z "$(git status --porcelain)" ]; then
  echo "no changes on $BRANCH, nothing to do"
  exit 0
fi

git add -A

if git diff --cached --quiet; then
  echo "nothing staged after add, nothing to do"
  exit 0
fi

STAMP="$(date +%Y-%m-%d)"
SHORTSTAT="$(git diff --cached --stat | tail -n 20)"
TOTALS="$(git diff --cached --numstat | awk '{a+=$1; d+=$2; n++} END {printf "共 %d 个文件，+%d 行，-%d 行", n, a, d}')"

# 写一份人可读的当日摘要，随提交一起入库
SUMMARY_DIR="docs/notes/daily"
mkdir -p "$SUMMARY_DIR"
{
  echo "# 每日变更 $STAMP"
  echo
  echo "- 分支：\`$BRANCH\`"
  echo "- 生成时间：$(date '+%Y-%m-%d %H:%M')"
  echo "- 统计：$TOTALS"
  echo
  echo "## 变更文件"
  echo
  echo '```'
  git diff --cached --stat
  echo '```'
} > "$SUMMARY_DIR/$STAMP.md"

git add -A

if ! git commit -q -m "chore(daily): auto snapshot $STAMP" -m "$SHORTSTAT"; then
  echo "ERROR: commit failed"
  osascript -e 'display notification "自动提交失败，请看日志" with title "alarm-clock"' 2>/dev/null
  exit 1
fi

HEAD_SHORT="$(git rev-parse --short HEAD)"
echo "committed $HEAD_SHORT ($TOTALS)"

# 推送：GitHub(origin) 与 Gitee(gitee) 都要推，防止两边分叉
PUSH_OK=""
for REMOTE in origin gitee; do
  git remote get-url "$REMOTE" >/dev/null 2>&1 || continue
  if git push -q "$REMOTE" "$BRANCH"; then
    PUSH_OK="$PUSH_OK $REMOTE"
  else
    echo "push to $REMOTE rejected, trying pull --rebase"
    if git pull -q --rebase "$REMOTE" "$BRANCH" && git push -q "$REMOTE" "$BRANCH"; then
      PUSH_OK="$PUSH_OK $REMOTE(rebase)"
    else
      echo "ERROR: push to $REMOTE failed, commit kept locally"
      osascript -e 'display notification "推送失败，提交已保存在本地" with title "alarm-clock"' 2>/dev/null
      exit 1
    fi
  fi
done

echo "pushed $HEAD_SHORT to$PUSH_OK ($BRANCH)"
osascript -e "display notification \"已提交 $HEAD_SHORT 到 GitHub 与 Gitee\" with title \"alarm-clock\"" 2>/dev/null
exit 0
