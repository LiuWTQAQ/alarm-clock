#pragma once
// ---- 天气 ----
#define CITY_NAME "北京"
#define CITY_LAT  39.9042f
#define CITY_LON  116.4074f
#define WEATHER_REFRESH_MS (30UL * 60UL * 1000UL)   // 30 min

// ---- 云端 ----
// 云端（天气代理、语音等走这个域名）
#define CLOUD_BASE "https://alarm.yoshino-liuwt.top"

#define DEV_ID     "a943f8"          // 与服务器 devices.id 一致
#define MQTT_TOPIC_CMD    "alarm/" DEV_ID "/cmd"
#define MQTT_TOPIC_STATE  "alarm/" DEV_ID "/state"
#define MQTT_TOPIC_EVENT  "alarm/" DEV_ID "/event"
#define MQTT_TOPIC_ACK    "alarm/" DEV_ID "/ack"
#define MQTT_TOPIC_ONLINE "alarm/" DEV_ID "/online"
