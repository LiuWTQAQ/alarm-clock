#pragma once
#include <time.h>

namespace net_time {
void begin();                 // non-blocking: starts Wi-Fi + SNTP
void loop();                  // call from loop(), ~free
bool connected();             // Wi-Fi up
bool timeOk();                // SNTP has produced a sane date
int  rssi();                  // dBm, 0 when not connected
const char* ssid();           // SSID currently in use (configured one when offline)
const char* pass();           // configured password (for keeping it when the form is blank)
bool setCredentials(const char* ssid, const char* pass);  // save to NVS + reconnect
bool localTime(struct tm& out);
}
