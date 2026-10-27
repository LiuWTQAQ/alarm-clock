#pragma once
#include <time.h>

namespace ui {
void begin();
void loop();                       // kept for main.cpp compatibility (no-op)

void renderAll();                  // full repaint of the current page
void tick(const struct tm& t);     // once per second: clock + date + progress bar
void setWifi(bool up, int rssi, const char* ssid = nullptr);
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
void debugDumpScreen();
void debugDumpFull();

// 供云端上报使用的只读访问器
const char* clockText();   // "13:37:49"
int         tempC();       // 当前温度（摄氏）
const char* condText();    // "多云"
int         aqi();         // -1 表示未知
void        redraw();      // 重画当前页（设置变更后调用）     // 320x240 hex dump for off-device inspection  // temporary: read the clock area back from the panel

}  // namespace ui
