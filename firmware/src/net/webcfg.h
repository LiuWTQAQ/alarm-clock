#pragma once

// Tiny configuration page served by the clock itself:
//   http://<device-ip>/   or   http://alarm.local/
// Lets the user pick the weather city and upload a background JPEG without
// re-flashing. Everything is persisted (city in NVS, image in LittleFS).
namespace webcfg {

void begin();
void loop();
bool started();

}  // namespace webcfg
