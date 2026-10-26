#pragma once
#include <Arduino.h>

// User-changeable settings, persisted in NVS so a city change does not need a
// firmware re-flash (the web page writes here).
namespace settings {

struct City {
  const char* name;
  float       lat;
  float       lon;
};

void        begin();
int         cityCount();
const City& cityAt(int i);
int         cityIndex();
void        setCityIndex(int i);
const char* cityName();
float       lat();
float       lon();

}  // namespace settings
