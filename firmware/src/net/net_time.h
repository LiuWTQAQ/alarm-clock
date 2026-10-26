#pragma once
#include <time.h>

namespace net_time {
void begin();                 // non-blocking: starts Wi-Fi + SNTP
void loop();                  // call from loop(), ~free
bool connected();             // Wi-Fi up
bool timeOk();                // SNTP has produced a sane date
int  rssi();                  // dBm, 0 when not connected
bool localTime(struct tm& out);
}
