// alarm-clock: composition root (init order + scheduling only)
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "ui/ui.h"
#include "net/net_time.h"
#include "net/weather.h"
#include "net/webcfg.h"
#include "settings.h"
#include "core/alarms.h"
#include "holidays.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  settings::begin();
  alarms::begin();
  Serial.printf("\n=== alarm-clock ===  city=%s\n", settings::cityName());

  ui::begin();              // screen first, so we have feedback even without Wi-Fi
  ui::setCity(settings::cityName());

  net_time::begin();        // Wi-Fi + SNTP
  weather::begin();         // first fetch ~2 s after boot
}

static void debugKeys() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
  case 'w': ui::tap(80, 120);  break;   // home -> weather detail
  case 'a': ui::tap(240, 120); break;   // home -> alarm list
  case 'c': ui::tap(80, 185);  break;   // home -> smart home
  case 'e': ui::tap(40, 50);   break;   // list -> edit first alarm
  case 'b': ui::back();        break;   // back to home
  case 's': ui::tap(240, 187); break;   // home -> settings
  case 't': ui::tap(160, 54);  break;   // toggle 12/24 h
  case 'l': ui::tap(160, 104); break;   // toggle language
  case 'x': ui::tap(160, 137); break;   // toggle C / F
  case 'W': ui::tap(160, 50); break;
  case 'm':
    Serial.printf("[mem] free %u B (%.1f KB), min %u B, largest block %u B\n",
                  ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0,
                  ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    break;
  case 'H': {
    const int t[5][3] = {{2026,10,26},{2026,10,1},{2026,9,20},{2026,10,10},{2026,5,4}};
    for (auto& d : t)
      Serial.printf("[cal] %04d-%02d-%02d workday=%d holiday=%d makeup=%d\n",
                    d[0], d[1], d[2], holidays::isWorkday(d[0], d[1], d[2]),
                    holidays::isHoliday(d[0], d[1], d[2]),
                    holidays::isMakeupWorkday(d[0], d[1], d[2]));
    break;
  }   // home clock -> world clock
  case 'C': settings::setCityIndex(5); ui::setCity(settings::cityName()); weather::restart(); break;  // 成都
  case 'p': ui::debugDumpClock(); break;  // dump clock sprite
  case 'q': ui::debugDumpScreen(); break; // dump 320x100 strip
  case 'f': ui::debugDumpFull();   break; // dump the whole frame
  default: break;
  }
}

void loop() {
  net_time::loop();
  debugKeys();
  weather::loop();
  if (net_time::connected() && !webcfg::started()) webcfg::begin();
  if (webcfg::started()) webcfg::loop();
  ui::loop();

  // Wi-Fi state -> header + bottom line
  static bool lastUp = false;
  static int  lastRssi = 0;
  bool up = net_time::connected();
  int  rssi = net_time::rssi();
  if (up != lastUp || rssi != lastRssi) {
    lastUp = up; lastRssi = rssi;
    ui::setWifi(up, rssi);
  }

  // clock, once per second
  static int lastSec = -1;
  struct tm t;
  if (net_time::localTime(t) && t.tm_sec != lastSec) {
    lastSec = t.tm_sec;
    ui::tick(t);
  }

  // weather, only when it actually changes
  static int  lastTemp = -999;
  static int  lastAqi  = -999;
  static char lastCond[16] = "";
  if (weather::valid() &&
      (weather::tempC() != lastTemp || strcmp(lastCond, weather::cond()) != 0)) {
    lastTemp = weather::tempC();
    strncpy(lastCond, weather::cond(), sizeof(lastCond) - 1);
    lastCond[sizeof(lastCond) - 1] = 0;
    ui::setWeather(lastTemp, lastCond);
    Serial.printf("[ui] weather %d C %s\n", lastTemp, lastCond);
  }
  if (weather::aqi() != lastAqi) {
    lastAqi = weather::aqi();
    ui::setAqi(lastAqi, weather::aqiText());
  }
}
