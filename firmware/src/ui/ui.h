#pragma once
#include <time.h>

namespace ui {
void begin();
void loop();                       // kept for main.cpp compatibility (no-op now)

void renderAll();                  // full repaint (after a background change)
void tick(const struct tm& t);     // once per second: clock + date + progress bar
void setWifi(bool up, int rssi);
void setWeather(int tempC, const char* cond);
void setAqi(int aqi, const char* label);
void setCity(const char* name);
void setBackground();              // reload /bg.jpg from LittleFS and repaint
}  // namespace ui
