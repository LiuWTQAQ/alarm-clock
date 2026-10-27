#pragma once
#include <Arduino.h>

namespace mqttc {
void begin();            // 启动（未配置 MQTT_HOST 时直接返回）
void loop();             // 维护连接、定时上报、处理下行命令
bool online();           // 是否已连上 broker
void publishState();     // 立即上报一次状态
const char* lastError(); // 最近一次失败原因（调试用）
}
