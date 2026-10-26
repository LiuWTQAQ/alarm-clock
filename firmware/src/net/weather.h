#pragma once
namespace weather {
void begin();
void restart();       // city changed: refetch immediately
void loop();          // blocking HTTPS GET, ~2-4 s, every 30 min
bool valid();
unsigned long lastUpdate();   // millis() of the last successful fetch, 0 = never
int  tempC();
const char* cond();   // "晴" / "多云" / "小雨" / "霾" / "沙尘" ...
int  aqi();           // US AQI, -1 = unknown
int  apparentC();     // feels-like temperature, -99 = unknown
int  humidity();      // relative humidity %, -1 = unknown
const char* textForCode(int code);
int  dayCode(int i);  // i = 0 tomorrow, 1, 2
int  dayMax(int i);
int  dayMin(int i);
const char* aqiText();// "优" / "良" / "轻度" / "中度" / "重度" / "严重"
}
