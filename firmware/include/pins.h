#pragma once

// ---- TFT (ILI9341, SPI) ----
// 注意：这 6 个引脚必须与 platformio.ini 里的 -DTFT_xxx 完全一致
#define PIN_TFT_CS    10
#define PIN_TFT_DC     8
#define PIN_TFT_RST    9
#define PIN_TFT_MOSI  11
#define PIN_TFT_SCLK  12
#define PIN_TFT_MISO  13
#define PIN_TFT_BL     7   // 若背光直接接 3.3V，则本行不用