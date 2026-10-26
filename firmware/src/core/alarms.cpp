#include "alarms.h"
#include "holidays.h"
#include <time.h>
#include <Preferences.h>

namespace alarms {
namespace {

Alarm      s_a[kMax];
int        s_n = 0;
Preferences prefs;

void setAlarm(Alarm& a, uint8_t h, uint8_t m, uint8_t mask, bool on, const char* label) {
  a.hour = h;
  a.min  = m;
  a.mask = mask;
  a.on   = on;
  strncpy(a.label, label, sizeof(a.label) - 1);
  a.label[sizeof(a.label) - 1] = 0;
}

void defaults() {
  s_n = 1;
  setAlarm(s_a[0], 7, 30, 0x1F, true, "起床");
}

}  // namespace

void begin() {
  prefs.begin("alm", false);
  s_n = prefs.getInt("n", 0);
  size_t got = prefs.getBytesLength("list");
  if (s_n <= 0 || s_n > kMax || got != sizeof(Alarm) * (size_t)s_n) {
    s_n = 0;
    defaults();
  } else {
    prefs.getBytes("list", s_a, got);
  }
  prefs.end();
}

int    count()   { return s_n; }
Alarm* at(int i) { return (i >= 0 && i < s_n) ? &s_a[i] : nullptr; }

int add() {
  if (s_n >= kMax) return -1;
  Alarm& a = s_a[s_n];
  setAlarm(a, 7, 0, 0x7F, true, "");
  s_n++;
  save();
  return s_n - 1;
}

void removeAt(int i) {
  if (i < 0 || i >= s_n) return;
  for (int k = i; k < s_n - 1; k++) s_a[k] = s_a[k + 1];
  s_n--;
  save();
}

void save() {
  prefs.begin("alm", false);
  prefs.putInt("n", s_n);
  prefs.putBytes("list", s_a, sizeof(Alarm) * (size_t)s_n);
  prefs.end();
}

const char* repeatText(uint8_t mask, char* buf, int n) {
  if (mask == 0)                   snprintf(buf, n, "一次");
  else if (mask == 0x1F)           snprintf(buf, n, "工作日");
  else if (mask == 0x60)           snprintf(buf, n, "周末");
  else if (mask == 0x7F)           snprintf(buf, n, "每天");
  else                             snprintf(buf, n, "自定义");
  return buf;
}

bool next(int nowHour, int nowMin, int year, int mon, int day, int wday, Alarm& out) {
  int now = nowHour * 60 + nowMin;
  int best = 100000;
  bool have = false;
  for (int i = 0; i < s_n; i++) {
    Alarm& a = s_a[i];
    if (!a.on) continue;
    int t = a.hour * 60 + a.min;
    int bestDelta = -1;
    for (int d = 0; d < 8; d++) {
      int wd = (wday + d) % 7;              // 0 = Sunday
      int bit = (wd + 6) % 7;               // mask bit 0 = Monday
      if (a.mask != 0 && !(a.mask & (1 << bit))) continue;

      struct tm cd = {};
      cd.tm_year = year - 1900; cd.tm_mon = mon - 1; cd.tm_mday = day + d; cd.tm_hour = 12;
      mktime(&cd);
      // 工作日模式：跳过周末与法定节假日，但调休上班日照常响
      if (a.mask == 0x1F && !holidays::isWorkday(cd.tm_year + 1900, cd.tm_mon + 1, cd.tm_mday))
        continue;

      int delta = d * 1440 + t - now;
      if (delta <= 0) continue;
      bestDelta = delta;
      break;
    }
    if (bestDelta < 0) continue;
    if (bestDelta < best) { best = bestDelta; out = a; have = true; }
  }
  return have;
}

}  // namespace alarms
