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
int         findCity(const char* name);   // -1 when unknown
const char* cityName();
float       lat();
float       lon();

// Display preferences (settings page writes these)
bool        hour12();
void        setHour12(bool v);
uint8_t     lang();          // 0 = Chinese, 1 = English
void        setLang(uint8_t v);
bool        fahrenheit();
void        setFahrenheit(bool v);

}  // namespace settings
