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
// ---- 外设 ----
#define PIN_BTN       1    // 顶部按键（内部上拉，按下接地）
#define PIN_RELAY    14    // 单路继电器，高电平吸合
#define PIN_BUZ      15    // 有源蜂鸣器
#define PIN_IR_RX     2    // VS1838B 输出
#define PIN_IR_TX    21    // 红外发射（经 S8050）
#define PIN_I2S_BCLK  4    // MAX98357A
#define PIN_I2S_LRC   5
#define PIN_I2S_DIN   6
#define PIN_MIC_SCK  18    // INMP441（第二路 I2S，暂未使用）
#define PIN_MIC_WS   38
#define PIN_MIC_SD   39
#define PIN_MIC_BTN  40    // 语音按键
