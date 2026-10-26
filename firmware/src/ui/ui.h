#pragma once
#include <time.h>

namespace ui {
void begin();
void loop();                       // kept for main.cpp compatibility (no-op)

void renderAll();                  // full repaint of the current page
void tick(const struct tm& t);     // once per second: clock + date + progress bar
void setWifi(bool up, int rssi);
void setWeather(int tempC, const char* cond);
void setAqi(int aqi, const char* label);
void setCity(const char* name);
void setBackground();              // reload /bg.jpg from LittleFS and repaint
void clearBackground();            // delete /bg.jpg and go back to the plain black layout

// Touch hooks. Call tap() from the XPT2046 driver once the panel is fitted;
// until then the same calls can be driven from the serial console.
void tap(int x, int y);
void back();
void debugDumpClock();   // temporary: stream the clock sprite over serial
void debugDumpScreen();  // temporary: read the clock area back from the panel

}  // namespace ui
