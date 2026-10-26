#include "holidays.h"
#include <time.h>

namespace holidays {
namespace {

struct Span { int y, m0, d0, m1, d1; };

// 2026：元旦 1/1-1/3；春节 2/15-2/23；清明 4/4-4/6；劳动节 5/1-5/5；
// 端午 6/19-6/21；中秋 9/25-9/27；国庆 10/1-10/7
const Span kHoliday[] = {
  {2026, 1, 1, 1, 3},   {2026, 2, 15, 2, 23}, {2026, 4, 4, 4, 6},
  {2026, 5, 1, 5, 5},   {2026, 6, 19, 6, 21}, {2026, 9, 25, 9, 27},
  {2026, 10, 1, 10, 7},
};

// 调休上班：1/4、2/14、2/28、5/9、9/20、10/10
const Span kMakeup[] = {
  {2026, 1, 4, 1, 4},   {2026, 2, 14, 2, 14}, {2026, 2, 28, 2, 28},
  {2026, 5, 9, 5, 9},   {2026, 9, 20, 9, 20}, {2026, 10, 10, 10, 10},
};

template <size_t N>
bool inTable(const Span (&tab)[N], int y, int m, int d) {
  for (size_t i = 0; i < N; i++) {
    const Span& s = tab[i];
    if (s.y != y) continue;
    if (m < s.m0 || m > s.m1) continue;
    if (m == s.m0 && d < s.d0) continue;
    if (m == s.m1 && d > s.d1) continue;
    return true;
  }
  return false;
}

int weekdayOf(int y, int m, int d) {
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon  = m - 1;
  t.tm_mday = d;
  t.tm_hour = 12;
  mktime(&t);
  return t.tm_wday;   // 0 = Sunday
}

}  // namespace

bool isHoliday(int y, int m, int d)       { return inTable(kHoliday, y, m, d); }
bool isMakeupWorkday(int y, int m, int d) { return inTable(kMakeup, y, m, d); }

bool isWorkday(int y, int m, int d) {
  if (isMakeupWorkday(y, m, d)) return true;
  if (isHoliday(y, m, d)) return false;
  int wd = weekdayOf(y, m, d);
  return wd >= 1 && wd <= 5;
}

}  // namespace holidays
