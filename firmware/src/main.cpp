#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n[probe] ---------- 配置检查 ----------");
#ifdef TFT_CS
  Serial.printf("[probe] TFT_CS   = %d\n", TFT_CS);
  Serial.printf("[probe] TFT_DC   = %d\n", TFT_DC);
  Serial.printf("[probe] TFT_RST  = %d\n", TFT_RST);
  Serial.printf("[probe] TFT_MOSI = %d\n", TFT_MOSI);
  Serial.printf("[probe] TFT_SCLK = %d\n", TFT_SCLK);
  Serial.printf("[probe] SPI_FREQ = %d\n", SPI_FREQUENCY);
#else
  Serial.println("[probe] TFT_CS 未定义 → build_flags 没生效！");
#endif

  tft.init();
  Serial.println("[probe] init 完成");

  // 慢速、逐色刷屏：如果颜色能变，说明 SPI 数据通路是通的
  for (int i = 0; i < 3; i++) {
    tft.fillScreen(i == 0 ? TFT_RED : (i == 1 ? TFT_GREEN : TFT_BLUE));
    Serial.printf("[probe] fill %s\n", i == 0 ? "RED" : (i == 1 ? "GREEN" : "BLUE"));
    delay(1200);
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SPI OK", 20, 40, 4);
  Serial.println("[probe] 文字已发送");
}

void loop() {
  static uint32_t t = 0;
  if (millis() - t > 2000) {
    t = millis();
    Serial.println("[probe] alive");
  }
}