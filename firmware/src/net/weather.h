#pragma once
namespace weather {
void begin();
void restart();       // city changed: refetch immediately
void loop();          // blocking HTTPS GET, ~2-4 s, every 30 min
bool valid();
int  tempC();
const char* cond();   // "晴" / "多云" / "小雨" / "霾" / "沙尘" ...
int  aqi();           // US AQI, -1 = unknown
const char* aqiText();// "优" / "良" / "轻度" / "中度" / "重度" / "严重"
}
