// alarm-clock: composition root (init order + scheduling only)
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "ui/ui.h"
#include "net/net_time.h"
#include "net/weather.h"
#include "net/webcfg.h"
#include "settings.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  settings::begin();
  Serial.printf("\n=== alarm-clock ===  city=%s\n", settings::cityName());

  ui::begin();              // screen first, so we have feedback even without Wi-Fi
  ui::setCity(settings::cityName());

  net_time::begin();        // Wi-Fi + SNTP
  weather::begin();         // first fetch ~2 s after boot
}

void loop() {
  net_time::loop();
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
