// MQTT 客户端：设备 ⇄ 云端（alarm/{dev}/cmd 下行，state/event/ack/online 上行）
#include "mqttc.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "pins.h"
#include "secrets.h"
#include "settings.h"
#include "ui/ui.h"
#include "net/net_time.h"
#include "net/weather.h"
#include "core/alarms.h"

namespace {
WiFiClient   s_net;
PubSubClient s_mq;
uint32_t     s_lastTry = 0;      // 上次重连尝试
uint32_t     s_backoff = 3000;   // 退避
uint32_t     s_lastState = 0;
int          s_rev = 0;
bool         s_light = false;
bool         s_acOn = false;
int          s_acTemp = 26;
char         s_err[48] = "";
const uint32_t kStateEveryMs = 20000;

void publishJson(const char* topic, JsonDocument& doc, bool retained = false) {
  char buf[1024];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  s_mq.publish(topic, (const uint8_t*)buf, n, retained);
}

void sendAck(const char* id, bool ok, const char* err) {
  JsonDocument d;
  d["id"] = id ? id : "";
  d["ok"] = ok;
  if (!ok) d["err"] = err ? err : "error";
  char topic[48];
  snprintf(topic, sizeof(topic), "alarm/%s/ack", DEV_ID);
  publishJson(topic, d);
}
}  // namespace

namespace mqttc {

bool online() { return s_mq.connected(); }
const char* lastError() { return s_err; }

static long weatherEpoch() {
  unsigned long ms = weather::lastUpdate();
  if (!ms) return 0;
  time_t now = time(nullptr);
  if (now < 1600000000) return 0;      // 还没对上时间
  long ago = (long)((millis() - ms) / 1000);
  return (long)now - ago;
}

void publishState() {
  if (!s_mq.connected()) return;
  JsonDocument d;
  d["ts"]   = (long)time(nullptr);
  d["rev"]  = s_rev;
  d["fw"]   = "0.2.0";
  d["clock"] = ui::clockText();

  JsonObject alarm = d["alarm"].to<JsonObject>();
  alarm["on"] = alarms::anyOn();
  alarm["n"]  = alarms::count();

  JsonArray arr = d["alarms"].to<JsonArray>();
  for (int i = 0; i < alarms::count(); i++) {
    alarms::Alarm* a = alarms::at(i);
    JsonObject o = arr.add<JsonObject>();
    o["h"] = a->hour; o["m"] = a->min; o["mask"] = a->mask;
    o["on"] = a->on;  o["label"] = a->label;
  }

  JsonObject light = d["light"].to<JsonObject>();
  light["on"] = s_light;

  JsonObject ac = d["ac"].to<JsonObject>();
  ac["on"] = s_acOn;
  ac["temp"] = s_acTemp;

  JsonObject env = d["env"].to<JsonObject>();
  env["t"] = ui::tempC();
  env["cond"] = ui::condText();
  env["aqi"] = ui::aqi();
  env["city"] = settings::cityName();
  env["ts"] = weatherEpoch();          // 最近一次成功抓取天气的时间（epoch 秒）

  JsonObject net = d["net"].to<JsonObject>();
  net["rssi"] = WiFi.RSSI();
  net["ssid"] = net_time::ssid();
  net["ip"]   = WiFi.localIP().toString();

  char topic[48];
  snprintf(topic, sizeof(topic), "alarm/%s/state", DEV_ID);
  publishJson(topic, d, true);
  s_lastState = millis();
}

static void handleCmd(const char* json) {
  JsonDocument d;
  if (deserializeJson(d, json)) return;
  const char* id = d["id"] | "";
  const char* op = d["op"] | "";
  bool changed = false;

  if (!strcmp(op, "light.set")) {
    s_light = d["on"] | false;
    digitalWrite(PIN_RELAY, s_light ? HIGH : LOW);
    Serial.printf("[mqtt] light -> %d\n", s_light);
    sendAck(id, true, nullptr);
    changed = true;
  } else if (!strcmp(op, "ac.set")) {
    if (d["on"].is<bool>())   s_acOn = d["on"].as<bool>();
    if (d["temp"].is<int>())  s_acTemp = d["temp"].as<int>();
    Serial.printf("[mqtt] ac -> %d / %d\n", s_acOn, s_acTemp);
    sendAck(id, true, nullptr);
    changed = true;
  } else if (!strcmp(op, "wifi.set")) {
    const char* ssid = d["ssid"] | "";
    const char* pass = d["pass"] | "";
    if (!*ssid) { sendAck(id, false, "no_ssid"); return; }
    sendAck(id, true, nullptr);          // 先回执，再断网
    delay(80);
    net_time::setCredentials(ssid, pass);
    changed = true;
  } else if (!strcmp(op, "alarm.sync")) {
    alarms::Alarm tmp[alarms::kMax];
    int n = 0;
    for (JsonObject o : d["alarms"].as<JsonArray>()) {
      if (n >= alarms::kMax) break;
      const char* lb = o["label"] | "闹钟";
      if (!*lb) lb = "闹钟";
      alarms::setAlarm(tmp[n], (uint8_t)(o["h"] | 7), (uint8_t)(o["m"] | 0),
                       (uint8_t)(o["mask"] | 0), o["on"] | true, lb);
      n++;
    }
    alarms::replaceAll(tmp, n);
    Serial.printf("[mqtt] alarm.sync n=%d\n", n);
    sendAck(id, true, nullptr);
    ui::redraw();
    changed = true;
  } else if (!strcmp(op, "config.set")) {
    if (d["hour12"].is<bool>())     settings::setHour12(d["hour12"].as<bool>());
    if (d["fahrenheit"].is<bool>()) settings::setFahrenheit(d["fahrenheit"].as<bool>());
    if (d["lang"].is<const char*>()) settings::setLang(strcmp(d["lang"], "en") == 0 ? 1 : 0);
    if (d["city"].is<const char*>()) {
      int idx = settings::findCity(d["city"]);
      if (idx >= 0 && idx != settings::cityIndex()) {
        settings::setCityIndex(idx);
        Serial.printf("[mqtt] city -> %s\n", settings::cityName());
        weather::restart();
      }
    }
    sendAck(id, true, nullptr);
    ui::redraw();
    changed = true;
  } else {
    sendAck(id, false, "unknown_op");
    return;
  }
  if (changed) { s_rev++; publishState(); }
}

static void onMessage(char* topic, byte* payload, unsigned int len) {
  char buf[512];
  if (len >= sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, payload, len);
  buf[len] = 0;
  Serial.printf("[mqtt] <- %s\n", buf);
  handleCmd(buf);
}

static bool connect() {
  char lwt[48];
  snprintf(lwt, sizeof(lwt), "alarm/%s/online", DEV_ID);
  bool ok = s_mq.connect(DEV_ID, MQTT_USER, MQTT_PASS, lwt, 1, true,
                         "{\"online\":false}");
  if (!ok) {
    snprintf(s_err, sizeof(s_err), "state=%d", s_mq.state());
    return false;
  }
  char topic[48];
  snprintf(topic, sizeof(topic), "alarm/%s/cmd", DEV_ID);
  s_mq.subscribe(topic, 1);
  snprintf(topic, sizeof(topic), "alarm/%s/online", DEV_ID);
  s_mq.publish(topic, (const uint8_t*)"{\"online\":true}", 15, true);
  s_backoff = 3000;
  s_err[0] = 0;
  Serial.printf("[mqtt] connected to %s\n", MQTT_HOST);
  publishState();
  return true;
}

void begin() {
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  s_mq.setClient(s_net);              // 必须先绑定底层连接，否则 connect() 空指针
  s_mq.setServer(MQTT_HOST, MQTT_PORT);
  s_mq.setCallback(onMessage);
  s_mq.setBufferSize(1024);         // 命令/状态里带 alarms 数组也够用
  s_mq.setKeepAlive(30);
}

void loop() {
  if (!strlen(MQTT_HOST)) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!s_mq.connected()) {
    if (millis() - s_lastTry < s_backoff) return;
    s_lastTry = millis();
    if (!connect()) {
      s_backoff = s_backoff < 30000 ? s_backoff * 2 : 60000;
      Serial.printf("[mqtt] retry in %lu ms (%s)\n", s_backoff, s_err);
    }
    return;
  }
  s_mq.loop();
  if (millis() - s_lastState > kStateEveryMs) publishState();
}

}  // namespace mqttc
