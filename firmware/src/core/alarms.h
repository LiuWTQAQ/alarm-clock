#pragma once
#include <Arduino.h>

// Alarm list, persisted in NVS. Kept deliberately small: six alarms is enough
// for a bedside clock and keeps the list page on one screen.
namespace alarms {

struct Alarm {
  uint8_t hour;
  uint8_t min;
  uint8_t mask;      // bit0..bit6 = Monday..Sunday, 0 = once
  bool    on;
  char    label[10];
};

const int kMax = 5;

void   begin();
int    count();
Alarm* at(int i);
int    add();                 // returns index of the new alarm
void   removeAt(int i);
void   save();

// "每天" / "工作日" / "周末" / "一次" / "自定义"
const char* repeatText(uint8_t mask, char* buf, int n);

// Earliest enabled alarm strictly after now; false when none.
// Earliest enabled alarm strictly after now. Workday alarms skip weekends,
// statutory holidays and honour the published make-up workdays.
bool next(int nowHour, int nowMin, int year, int mon, int day, int wday, Alarm& out);

}  // namespace alarms
