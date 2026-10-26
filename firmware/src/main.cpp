#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "pins.h"

TFT_eSPI tft = TFT_eSPI();

static const uint16_t C_BG     = TFT_BLACK;
static const uint16_t C_FG     = TFT_WHITE;
static const uint16_t C_ACCENT = 0x07FF;   // 青
static const uint16_t C_DIM    = 0x8410;   // 灰
static const uint16_t C_WARN   = 0xFD20;   // 橙

uint8_t  page   = 0;
uint32_t tPage  = 0;
uint32_t tClock = 0;
const uint32_t PAGE_MS = 5000;

void pageClockUI(bool full);
void pageColorBars();
void pageFonts();
void pageShapes();

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[boot] TFT self-test");

  // 板子真实身份：这里会暴露 Flash/PSRAM 是否被正确识别
  Serial.printf("[boot] chip=%s rev=%d cores=%d\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
  Serial.printf("[boot] flash=%u MB  psram=%u KB  heap=%u B\n",
                ESP.getFlashChipSize() / 1048576,
                ESP.getPsramSize() / 1024,
                ESP.getFreeHeap());

#ifdef PIN_TFT_BL
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);   // 背光常亮
#endif

  tft.init();
  tft.setRotation(1);               // 1 = 横屏 320x240
  tft.setSwapBytes(true);           // 以后贴图用得上
  tft.fillScreen(C_BG);

  Serial.printf("[boot] tft = %d x %d, rotation %d\n",
                tft.width(), tft.height(), tft.getRotation());

  pageClockUI(true);
  tPage = tClock = millis();
}

void loop() {
  // 第 0 页每秒局部刷新，演示"不闪屏的局部重绘"
  if (page == 0 && millis() - tClock >= 1000) {
    tClock = millis();
    pageClockUI(false);
  }

  if (millis() - tPage >= PAGE_MS) {
    tPage = millis();
    page  = (page + 1) % 4;
    switch (page) {
      case 0: pageClockUI(true);  break;
      case 1: pageColorBars();    break;
      case 2: pageFonts();        break;
      case 3: pageShapes();       break;
    }
    Serial.printf("[test] page %u\n", page);
  }
}

// ---------- 第 0 页：闹钟界面草图 ----------
void pageClockUI(bool full) {
  if (full) {
    tft.fillScreen(C_BG);
    // 顶部状态条
    tft.fillRect(0, 0, tft.width(), 22, 0x1082);
    tft.setTextColor(C_DIM, 0x1082);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("WiFi", 6, 4, 2);
    tft.drawString("ALARM ON", 150, 4, 2);
    tft.drawString("Fri 24 Oct", 240, 4, 2);

    // 右下角天气区
    tft.fillCircle(268, 78, 18, TFT_YELLOW);
    for (int i = 0; i < 8; i++) {
      float a = i * PI / 4.0f;
      int x1 = 268 + (int)(24 * cos(a)), y1 = 78 + (int)(24 * sin(a));
      int x2 = 268 + (int)(32 * cos(a)), y2 = 78 + (int)(32 * sin(a));
      tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
    }
    tft.setTextColor(C_FG, C_BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("23C", 318, 104, 4);
    tft.setTextColor(C_DIM, C_BG);
    tft.drawString("CLEAR", 318, 132, 2);

    // 底部提示条
    tft.fillRect(0, 196, tft.width(), 44, 0x0861);
    tft.setTextColor(C_DIM, 0x0861);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("CLICK: STOP", 8, 204, 2);
    tft.drawString("HOLD: SNOOZE", 8, 222, 2);
    tft.drawString("2X: LAMP", 210, 204, 2);
  }

  // 时间：先用开机时间模拟，后面换成 SNTP
  uint32_t s   = (millis() / 1000) % 86400;
  uint32_t tot = (7 * 3600 + 30 * 60 + s) % 86400;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", tot / 3600, (tot / 60) % 60);

  tft.fillRect(8, 48, 190, 56, C_BG);        // 只擦时间区
  tft.setTextColor(C_FG, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(buf, 10, 48, 6);

  tft.fillRect(8, 112, 190, 24, C_BG);
  tft.setTextColor(C_DIM, C_BG);
  tft.drawString("NEXT ALARM 06:45", 10, 112, 2);
}

// ---------- 第 1 页：彩条（查接线、查颜色顺序）----------
void pageColorBars() {
  const uint16_t bars[8] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE,
                            TFT_BLACK, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA};
  const char* names[8] = {"R", "G", "B", "W", "K", "Y", "C", "M"};
  int w = tft.width() / 8;
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < 8; i++) {
    tft.fillRect(i * w, 0, w, tft.height(), bars[i]);
    tft.setTextColor(i == 4 ? TFT_WHITE : TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(names[i], i * w + w / 2, 4, 2);
  }
}

// ---------- 第 2 页：字体与色板 ----------
void pageFonts() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_FG, TFT_BLACK);
  tft.drawString("Font 1: 0123456789", 6, 6, 1);
  tft.drawString("Font 2: 0123456789", 6, 24, 2);
  tft.drawString("Font 4: 0123", 6, 48, 4);
  tft.drawString("F6", 6, 82, 6);
  tft.drawString("F7", 80, 82, 7);
  tft.drawString("F8", 170, 82, 8);

  // 色板
  for (int i = 0; i < 16; i++) {
    uint16_t c = tft.color565(i * 16, (15 - i) * 16, (i * 32) % 256);
    tft.fillRect(6 + i * 19, 200, 18, 30, c);
  }
}

// ---------- 第 3 页：图形（查 SPI 稳定性、有无撕裂）----------
void pageShapes() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(C_ACCENT, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Shapes / SPI check", 6, 4, 2);

  tft.drawCircle(60, 120, 40, TFT_CYAN);
  tft.fillCircle(60, 120, 12, TFT_CYAN);

  tft.drawRect(120, 80, 80, 80, TFT_GREEN);
  tft.fillRoundRect(220, 80, 80, 80, 10, 0x2945);

  for (int i = 0; i < 40; i++)
    tft.drawLine(0, 200 + i, tft.width(), 200 + i, tft.color565(i * 6, 0, 0));

  tft.drawTriangle(30, 300, 90, 250, 150, 300, TFT_MAGENTA);

  tft.setTextColor(C_WARN, TFT_BLACK);
  tft.drawString("320x240 ROT1", 210, 200, 2);
}