#include "weather.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "settings.h"

namespace weather {
namespace {

struct Entry { int code; const char* text; };
const Entry kMap[] = {
  {0, "晴"},    {1, "晴"},     {2, "多云"},  {3, "阴"},
  {45, "雾"},   {48, "雾"},
  {51, "小雨"}, {53, "小雨"},  {55, "小雨"},
  {56, "冻雨"}, {57, "冻雨"},
  {61, "小雨"}, {63, "中雨"},  {65, "大雨"},
  {66, "冻雨"}, {67, "冻雨"},
  {71, "小雪"}, {73, "中雪"},  {75, "大雪"}, {77, "小雪"},
  {80, "阵雨"}, {81, "阵雨"},  {82, "暴雨"},
  {85, "阵雪"}, {86, "阵雪"},
  {95, "雷雨"}, {96, "雷雨"},  {99, "雷雨"},
};

int      s_temp  = -1;
unsigned long s_lastUpdate = 0;
int      s_app   = -1;
int      s_hum   = -1;
int      s_dayCode[3] = {-1, -1, -1};
int      s_dayMax[3]  = {-999, -999, -999};
int      s_dayMin[3]  = {-999, -999, -999};
int      s_code  = -1;
const char* s_cond = "获取中";
int      s_aqi   = -1;
int      s_pm25  = -1;
int      s_pm10  = -1;
bool     s_valid = false;
uint32_t s_next  = 0;

float jsonNumber(const String& body, const char* key, int from) {
  int i = body.indexOf(key, from);
  if (i < 0) return NAN;
  i = body.indexOf(':', i);
  if (i < 0) return NAN;
  return body.substring(i + 1).toFloat();
}

const char* codeToText(int code) {
  for (const Entry& e : kMap)
    if (e.code == code) return e.text;
  return "多云";
}

// haze and dust are not WMO codes: derive them from particulate readings
void applyCond() {
  if (s_code < 0) { s_cond = "获取中"; return; }
  bool clearSky = (s_code <= 3);
  if (clearSky && s_pm10 > 300)      { s_cond = "沙尘"; return; }
  if (clearSky && s_pm25 > 75)       { s_cond = "霾";   return; }
  s_cond = codeToText(s_code);
}

bool fetchAqi();

bool httpGetJson(const char* url, JsonDocument& doc) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) { Serial.println("[wx] begin failed"); return false; }
  http.setTimeout(8000);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("[wx] http %d\n", httpCode);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  DeserializationError err = deserializeJson(doc, body);
  if (err) { Serial.printf("[wx] json %s\n", err.c_str()); return false; }
  return true;
}

bool fetchWeather() {
  char url[320];
  JsonDocument doc;

  // 首选云端代理：一次请求拿到天气 + 空气质量，云端按坐标缓存 30 分钟
  snprintf(url, sizeof(url), CLOUD_BASE "/api/weather?lat=%.4f&lon=%.4f",
           (double)settings::lat(), (double)settings::lon());
  if (!httpGetJson(url, doc)) {
    // 云端不可用时退回直连，保证断网/服务器故障时天气还能用
    Serial.println("[wx] cloud failed, direct");
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min"
             "&forecast_days=4&timezone=Asia%%2FShanghai",
             (double)settings::lat(), (double)settings::lon());
    if (!httpGetJson(url, doc)) return false;
  }

  JsonObject cur = doc["current"];
  if (cur.isNull()) { Serial.println("[wx] no current block"); return false; }

  s_temp = (int)lroundf(cur["temperature_2m"] | -99.0f);
  s_app  = (int)lroundf(cur["apparent_temperature"] | -99.0f);
  s_hum  = (int)(cur["relative_humidity_2m"] | -1);
  s_code = (int)(cur["weather_code"] | -1);

  JsonArray dcode = doc["daily"]["weather_code"];
  JsonArray dmax  = doc["daily"]["temperature_2m_max"];
  JsonArray dmin  = doc["daily"]["temperature_2m_min"];
  for (int i = 0; i < 3; i++) {
    int k = i + 1;                       // 0 is today, we want the next three
    if (k >= (int)dcode.size()) break;
    s_dayCode[i] = dcode[k] | -1;
    s_dayMax[i]  = (int)lroundf(dmax[k] | -999.0f);
    s_dayMin[i]  = (int)lroundf(dmin[k] | -999.0f);
  }

  // 云端把空气质量合并在同一个响应里；直连时没有 air 字段，回退到单独请求
  JsonObject air = doc["air"];
  if (!air.isNull()) {
    float a = air["us_aqi"] | -1.0f;
    if (a >= 0) {
      s_aqi  = (int)lroundf(a);
      s_pm25 = (int)lroundf(air["pm2_5"] | -1.0f);
      s_pm10 = (int)lroundf(air["pm10"] | -1.0f);
    }
  } else {
    fetchAqi();
  }
  return true;
}

bool fetchAqi() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  char url[260];
  snprintf(url, sizeof(url),
           "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f"
           "&current=us_aqi,pm2_5,pm10&timezone=Asia%%2FShanghai",
           (double)settings::lat(), (double)settings::lon());

  if (!http.begin(client, url)) { Serial.println("[aqi] begin failed"); return false; }
  http.setTimeout(8000);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("[aqi] http %d\n", httpCode);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  int cur = body.indexOf("\"current\":");
  if (cur < 0) { Serial.println("[aqi] no current block"); return false; }

  float a  = jsonNumber(body, "\"us_aqi\"", cur);
  float p2 = jsonNumber(body, "\"pm2_5\"", cur);
  float p10 = jsonNumber(body, "\"pm10\"", cur);
  if (isnan(a)) { Serial.println("[aqi] parse failed"); return false; }

  s_aqi  = (int)lroundf(a);
  s_pm25 = isnan(p2) ? -1 : (int)lroundf(p2);
  s_pm10 = isnan(p10) ? -1 : (int)lroundf(p10);
  return true;
}

}  // namespace

void begin() { s_next = millis() + 2000; }

void restart() {
  s_valid = false;
  s_app = -1;
  s_hum = -1;
  s_aqi   = -1;
  s_next  = millis() + 500;
  Serial.println("[wx] restart for new city");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { s_next = millis() + 3000; return; }
  if ((int32_t)(millis() - s_next) < 0) return;

  bool okWx  = fetchWeather();
  bool okAqi = (s_aqi >= 0);          // 云端代理会在同一次响应里带上空气质量
  if (!okAqi) okAqi = fetchAqi();
  bool ok    = okWx || okAqi;

  if (okWx) {
    applyCond();
    s_valid = true;
  s_lastUpdate = millis();
  }
  s_next = millis() + (ok ? WEATHER_REFRESH_MS : 60000UL);

  Serial.printf("[wx] %s  %s  %d C  %s | aqi %d %s  pm2.5 %d  pm10 %d\n",
                ok ? "ok" : "fail", settings::cityName(), s_temp, s_cond,
                s_aqi, aqiText(), s_pm25, s_pm10);
}

bool valid()        { return s_valid; }
unsigned long lastUpdate() { return s_lastUpdate; }
int  tempC()        { return s_temp; }
const char* cond()  { return s_cond; }
int  aqi()          { return s_aqi; }

int         apparentC() { return s_app; }
int         humidity()  { return s_hum; }

const char* textForCode(int code) {
  for (const Entry& e : kMap)
    if (e.code == code) return e.text;
  return "--";
}

int         dayCode(int i) { return (i >= 0 && i < 3) ? s_dayCode[i] : -1; }
int         dayMax(int i)  { return (i >= 0 && i < 3) ? s_dayMax[i] : -999; }
int         dayMin(int i)  { return (i >= 0 && i < 3) ? s_dayMin[i] : -999; }

const char* aqiText() {
  if (s_aqi < 0)   return "";
  if (s_aqi <= 50)  return "优";
  if (s_aqi <= 100) return "良";
  if (s_aqi <= 150) return "轻度";
  if (s_aqi <= 200) return "中度";
  if (s_aqi <= 300) return "重度";
  return "严重";
}

}  // namespace weather
