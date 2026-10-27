#include "net_time.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include "secrets.h"

namespace {

enum State { S_WIFI, S_TIME, S_OK };
State    s_state    = S_WIFI;
uint32_t s_lastTry  = 0;
uint32_t s_lastSync = 0;

char     s_ssid[33] = WIFI_SSID;
char     s_pass[65] = WIFI_PASS;

const char* kNtp[3] = {"ntp.aliyun.com", "ntp.tencent.com", "cn.pool.ntp.org"};
const char* kTz     = "CST-8";                 // Asia/Shanghai, no DST

void startSntp() {
  configTzTime(kTz, kNtp[0], kNtp[1], kNtp[2]);
}

bool haveSaneTime() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  return t.tm_year > 120;                      // > 2020
}

void loadCreds() {
  Preferences p;
  if (!p.begin("wifi", true)) return;
  String ssid = p.getString("ssid", "");
  String pass = p.getString("pass", "");
  p.end();
  if (ssid.length() && ssid.length() < sizeof(s_ssid)) {
    strncpy(s_ssid, ssid.c_str(), sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = 0;
    strncpy(s_pass, pass.c_str(), sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = 0;
  }
}

}  // namespace

void net_time::begin() {
  loadCreds();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);                        // keeps SPI/TFT timing stable
  WiFi.begin(s_ssid, s_pass);
  s_lastTry = millis();
  Serial.printf("[net] connecting to %s\n", s_ssid);
}

const char* net_time::ssid() {
  // 注意：WiFi.SSID() 返回临时 String，不能直接返回 c_str()（悬垂指针）
  static char buf[33];
  if (WiFi.status() == WL_CONNECTED) {
    String s = WiFi.SSID();
    strncpy(buf, s.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    return buf;
  }
  return s_ssid;
}

const char* net_time::pass() { return s_pass; }

bool net_time::setCredentials(const char* ssid, const char* pass) {
  if (!ssid || !*ssid) return false;
  Preferences p;
  if (p.begin("wifi", false)) {
    p.putString("ssid", ssid);
    p.putString("pass", pass ? pass : "");
    p.end();
  }
  strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
  s_ssid[sizeof(s_ssid) - 1] = 0;
  strncpy(s_pass, pass ? pass : "", sizeof(s_pass) - 1);
  s_pass[sizeof(s_pass) - 1] = 0;

  WiFi.disconnect(true, false);
  delay(200);
  WiFi.begin(s_ssid, s_pass);
  s_state = S_WIFI;
  s_lastTry = millis();
  Serial.printf("[net] credentials -> %s, reconnecting\n", s_ssid);
  return true;
}

void net_time::loop() {
  uint32_t now = millis();

  switch (s_state) {
    case S_WIFI:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[net] wifi ok  ip=%s  rssi=%d\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        startSntp();
        s_lastSync = now;
        s_state = S_TIME;
      } else if (now - s_lastTry > 15000) {
        s_lastTry = now;
        Serial.println("[net] retry wifi");
        WiFi.disconnect();
        WiFi.begin(s_ssid, s_pass);
      }
      break;

    case S_TIME:
      if (haveSaneTime()) {
        s_state = S_OK;
        Serial.println("[net] time synced");
      } else if (now - s_lastSync > 20000) {
        Serial.println("[net] sntp retry");
        startSntp();
        s_lastSync = now;
      }
      break;

    case S_OK:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[net] wifi lost");
        s_state = S_WIFI;
        s_lastTry = now;
      } else if (now - s_lastSync > 6UL * 3600UL * 1000UL) {
        startSntp();                           // resync every 6 h
        s_lastSync = now;
      }
      break;
  }
}

bool net_time::connected() { return WiFi.status() == WL_CONNECTED; }
bool net_time::timeOk()    { return s_state == S_OK; }
int  net_time::rssi()      { return connected() ? WiFi.RSSI() : 0; }

bool net_time::localTime(struct tm& out) {
  return getLocalTime(&out, 0) && out.tm_year > 120;
}
