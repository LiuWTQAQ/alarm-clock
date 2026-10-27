#pragma once
// copy to include/secrets.h (already in .gitignore) and fill in
#define WIFI_SSID "your-2.4g-ssid"
#define WIFI_PASS "your-password"

// ---- MQTT（填服务器的公网地址；没有就留空，固件会跳过联网控制）----
#define MQTT_HOST "alarm.yoshino-liuwt.top"
#define MQTT_PORT 1883
#define MQTT_USER "alarm"
#define MQTT_PASS "change-me"
