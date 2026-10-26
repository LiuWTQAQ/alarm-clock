#include "ui.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <math.h>
#include <string.h>
#include "ui_font_cn.h"

namespace ui {

namespace {

TFT_eSPI tft;
#define TFT_ROTATION 3

const int kTimeY = 24;
const int kTimeH = 56;

const uint16_t C_BG     = TFT_BLACK;
const uint16_t C_TEXT   = 0xFFFF;
const uint16_t C_DIM    = 0x8C71;
const uint16_t C_ACCENT = 0x07FF;
const uint16_t C_WARM   = 0xFD20;
const uint16_t C_CARD   = 0x18E3;
const uint16_t C_LINE   = 0x3186;
const uint16_t C_SAND   = 0xC5C0;
const uint16_t C_SNOW   = 0xBDF7;

// Off-screen box that pushes only the pixels that changed since the last
// frame. Nothing is ever cleared on the panel, so the clock digits never
// blank out while being redrawn.
struct Box {
  TFT_eSprite spr;
  uint16_t*   prev = nullptr;
  int         w = 0, h = 0;

  explicit Box(TFT_eSPI* t) : spr(t) {}

  bool init(int w_, int h_) {
    w = w_;
    h = h_;
    spr.setColorDepth(16);
    if (!spr.createSprite(w, h)) return false;
    prev = (uint16_t*)calloc((size_t)w * h, 2);
    return prev != nullptr;
  }
  void clear(uint16_t col) { spr.fillSprite(col); }

  void push(int x, int y) {
    uint16_t* img = (uint16_t*)spr.getPointer();
    for (int row = 0; row < h; row++) {
      uint16_t* a = img + row * w;
      uint16_t* b = prev + row * w;
      int c = 0;
      while (c < w) {
        if (a[c] != b[c]) {
          uint16_t col = a[c];
          int s0 = c;
          while (c < w && a[c] == col) c++;
          tft.drawFastHLine(x + s0, y + row, c - s0, col);
        } else {
          c++;
        }
      }
    }
    memcpy(prev, img, (size_t)w * h * 2);
  }
};

Box timeBox(&tft);
bool s_timeBox = false;
int  s_timeW = 0;
int  s_lastBarW = 0;
int  s_bars = -1;
bool s_hasBg = false;

int      s_h = 0, s_m = 0, s_s = 0;
bool     s_timeOk = false;
int      s_temp   = -1;
const char* s_cond = "获取中";
char     s_date[32] = "---";
char     s_city[16] = "北京";
bool     s_wifi   = false;
int      s_rssi   = 0;
int      s_aqi    = -1;
const char* s_aqiText = "";

// ---------- utf8 + cjk ----------
uint32_t utf8Next(const char*& p) {
  uint8_t c = (uint8_t)*p++;
  if (c < 0x80) return c;
  int n = ((c & 0xE0) == 0xC0) ? 1 : ((c & 0xF0) == 0xE0) ? 2 : 3;
  uint32_t v = c & (0x3F >> n);
  while (n-- > 0 && *p) v = (v << 6) | ((uint8_t)*p++ & 0x3F);
  return v;
}

const CnGlyph* cnLookup(const CnFont& f, uint32_t cp) {
  for (uint16_t i = 0; i < f.n; i++)
    if (f.g[i].cp == cp) return &f.g[i];
  return nullptr;
}

int cnWidth(const CnFont& f, const char* s) {
  int w = 0;
  const char* p = s;
  while (*p) { const CnGlyph* g = cnLookup(f, utf8Next(p)); w += g ? g->adv : f.ascent / 2; }
  return w;
}

void cnDraw(const CnFont& f, const char* s, int x, int yTop, uint16_t color) {
  const char* p = s;
  int pen = x;
  while (*p) {
    const CnGlyph* g = cnLookup(f, utf8Next(p));
    if (!g) { pen += f.ascent / 2; continue; }
    if (g->w && g->h) {
      int ox = pen + g->xoff, oy = yTop + g->yoff;
      int rb = (g->w + 7) / 8;
      for (int row = 0; row < g->h; row++) {
        const uint8_t* line = f.data + g->off + row * rb;
        int c = 0;
        while (c < g->w) {
          if (line[c >> 3] & (0x80 >> (c & 7))) {
            int start = c;
            while (c < g->w && (line[c >> 3] & (0x80 >> (c & 7)))) c++;
            tft.drawFastHLine(ox + start, oy + row, c - start, color);
          } else c++;
        }
      }
    }
    pen += g->adv;
  }
}

// One-pixel drop shadow keeps text readable when a photo is the background.
void textS(const CnFont& f, const char* s, int x, int y, uint16_t color) {
  if (s_hasBg) cnDraw(f, s, x + 1, y + 1, TFT_BLACK);
  cnDraw(f, s, x, y, color);
}

// Diagonal checkerboard that halves the photo brightness behind a text band.
void dimBand(int x, int y, int w, int h) {
  if (!s_hasBg) return;
  for (int j = 0; j < h; j++)
    for (int i = (j & 1); i < w; i += 2)
      tft.drawPixel(x + i, y + j, TFT_BLACK);
}

uint8_t condKind() {
  if (strstr(s_cond, "雷"))                       return 5;
  if (strstr(s_cond, "雪"))                       return 3;
  if (strstr(s_cond, "雾"))                       return 4;
  if (strstr(s_cond, "霾") || strstr(s_cond, "沙尘")) return 6;
  if (strstr(s_cond, "雨"))                       return 2;
  if (strcmp(s_cond, "晴") == 0)                  return 0;
  return 1;
}

uint16_t aqiColor(int a) {
  if (a <= 50)  return 0x07E0;
  if (a <= 100) return 0xFFE0;
  if (a <= 150) return 0xFD20;
  if (a <= 200) return 0xF800;
  if (a <= 300) return 0xF81F;
  return 0x8000;
}

// ---------- static shapes ----------
void cloudShape(TFT_eSPI& g, int cx, int cy, uint16_t col) {
  g.fillCircle(cx - 8, cy, 8, col);
  g.fillCircle(cx + 2, cy - 5, 10, col);
  g.fillCircle(cx + 11, cy + 2, 7, col);
  g.fillRect  (cx - 8, cy, 20, 8, col);
}

void drawWeatherIcon(TFT_eSPI& g, int cx, int cy, uint8_t kind) {
  switch (kind) {
  case 0:  // clear
    for (int a = 0; a < 8; a++) {
      float r = a * 3.14159265f / 4.0f;
      g.drawLine(cx + (int)(cosf(r) * 13), cy + (int)(sinf(r) * 13),
                 cx + (int)(cosf(r) * 19), cy + (int)(sinf(r) * 19), C_WARM);
    }
    g.fillCircle(cx, cy, 9, C_WARM);
    break;

  case 1:  // cloud
    cloudShape(g, cx, cy, C_TEXT);
    break;

  case 2:  // rain
    cloudShape(g, cx, cy - 6, C_TEXT);
    for (int i = -1; i <= 1; i++) {
      g.drawLine(cx + i * 8, cy + 8, cx + i * 8 - 3, cy + 15, C_ACCENT);
    }
    break;

  case 3:  // snow
    cloudShape(g, cx, cy - 6, C_TEXT);
    for (int i = -1; i <= 1; i++) {
      int fx = cx + i * 9, fy = cy + 12;
      g.drawFastHLine(fx - 3, fy, 7, C_SNOW);
      g.drawFastVLine(fx, fy - 3, 7, C_SNOW);
    }
    break;

  case 4:  // fog
    for (int i = 0; i < 3; i++)
      g.drawFastHLine(cx - 16, cy - 8 + i * 8, 32, C_DIM);
    break;

  case 5:  // thunder
    cloudShape(g, cx, cy - 8, C_TEXT);
    g.drawLine(cx + 3, cy + 4,  cx - 2, cy + 13, C_WARM);
    g.drawLine(cx - 2, cy + 13, cx + 4, cy + 13, C_WARM);
    g.drawLine(cx + 4, cy + 13, cx,     cy + 21, C_WARM);
    break;

  default:  // haze / dust
    cloudShape(g, cx, cy - 4, 0x9CD3);
    for (int i = 0; i < 4; i++)
      g.fillCircle(cx - 12 + i * 8, cy + 10 + (i & 1) * 4, 1, C_SAND);
    break;
  }
}

void bellShape(TFT_eSPI& g, int x, int y) {
  g.fillRoundRect(x - 9, y - 8, 18, 15, 4, C_WARM);
  g.fillRect     (x - 11, y + 7, 22, 3, C_WARM);
  g.fillCircle   (x, y + 12, 3, C_WARM);
}

// ---------- background photo ----------
bool jpgCb(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
  if (y >= (int)tft.height()) return 0;
  for (int i = 0; i < (int)w * (int)h; i++) bmp[i] = (bmp[i] >> 1) & 0x7BEF;  // 50 % dim
  tft.pushImage(x, y, w, h, bmp);
  return 1;
}

bool drawBgImg() {
  if (!LittleFS.begin(false)) return false;
  if (!LittleFS.exists("/bg.jpg")) return false;
  uint16_t iw = 0, ih = 0;
  int scale = 1;
  if (TJpgDec.getFsJpgSize(&iw, &ih, "/bg.jpg", LittleFS)) {
    while (iw / scale > 320 || ih / scale > 240) scale *= 2;
  }
  TJpgDec.setJpgScale(scale);
  TJpgDec.setCallback(jpgCb);
  // JRESULT 0 is JDR_OK, not "false"
  return TJpgDec.drawFsJpg(0, 0, "/bg.jpg", LittleFS) == JDR_OK;
}

// ---------- regions ----------
void drawBars() {
  int bars = 0;
  if (s_wifi) bars = (s_rssi > -55) ? 4 : (s_rssi > -65) ? 3 : (s_rssi > -75) ? 2 : 1;
  if (bars == s_bars) return;
  s_bars = bars;
  if (s_hasBg) dimBand(226, 2, 26, 22);
  for (int i = 0; i < 4; i++)
    tft.fillRect(228 + i * 5, 17 - i * 3, 3, 4 + i * 3,
                 (i < bars) ? C_ACCENT : C_LINE);
}

void drawHeader() {
  if (s_hasBg) dimBand(0, 0, 320, 25);
  else         tft.fillRect(0, 0, 320, 25, C_BG);
  tft.drawFastHLine(0, 24, 320, C_LINE);
  textS(cn18, s_date, 10, 2, C_DIM);
  textS(cn15, "闹钟已开", 314 - cnWidth(cn15, "闹钟已开"), 5, C_WARM);
  s_bars = -1;
  drawBars();
}

void paintTime() {
  char buf[12];
  if (!s_timeOk) snprintf(buf, sizeof(buf), "--:--:--");
  else           snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s_h, s_m, s_s);

  if (s_hasBg || !s_timeBox) {
    // Seven-segment digits are fixed width, so drawing straight over the old
    // value leaves no gaps; the shadow separates them from the photo.
    tft.setTextDatum(MC_DATUM);
    if (s_hasBg) {
      tft.setTextColor(TFT_BLACK);
      tft.drawString(buf, 161, kTimeY + kTimeH / 2 + 1, 7);
    }
    tft.setTextColor(C_TEXT, s_hasBg ? C_TEXT : C_BG);
    tft.drawString(buf, 160, kTimeY + kTimeH / 2, 7);
    return;
  }
  timeBox.clear(C_BG);
  timeBox.spr.setTextDatum(MC_DATUM);
  timeBox.spr.setTextColor(C_TEXT);
  timeBox.spr.drawString(buf, s_timeW / 2, kTimeH / 2, 7);
  timeBox.push(160 - s_timeW / 2, kTimeY);
}

void paintBar() {
  int w = s_timeOk ? (s_s * 300) / 59 : 0;
  if (w > s_lastBarW)      tft.fillRect(10 + s_lastBarW, 82, w - s_lastBarW, 3, C_ACCENT);
  else if (w < s_lastBarW) tft.fillRect(10 + w, 82, s_lastBarW - w, 3, C_LINE);
  s_lastBarW = w;
}

void drawWeatherCard() {
  tft.fillRoundRect(8, 90, 150, 74, 8, C_CARD);
  drawWeatherIcon(tft, 32, 126, condKind());

  cnDraw(cn15, s_city, 64, 92, C_DIM);

  char t[8];
  if (s_temp < 0) snprintf(t, sizeof(t), "--");
  else            snprintf(t, sizeof(t), "%d", s_temp);
  cnDraw(cn28, t, 64, 107, C_TEXT);

  int x = 64 + cnWidth(cn28, t) + 2;
  cnDraw(cn15, "°C", x, 120, C_WARM);
  x += cnWidth(cn15, "°C") + 4;
  cnDraw(cn15, s_cond, x, 121, C_DIM);

  x = 64;
  cnDraw(cn15, "空气", x, 142, C_DIM);
  x += cnWidth(cn15, "空气") + 4;
  char q[8];
  if (s_aqi < 0) snprintf(q, sizeof(q), "--");
  else           snprintf(q, sizeof(q), "%d", s_aqi);
  cnDraw(cn15, q, x, 142, C_TEXT);
  x += cnWidth(cn15, q) + 4;
  if (s_aqi >= 0) cnDraw(cn15, s_aqiText, x, 142, aqiColor(s_aqi));
}

void drawAlarmCard() {
  tft.fillRoundRect(166, 90, 146, 74, 8, C_CARD);
  bellShape(tft, 184, 124);

  cnDraw(cn15, "下一个闹钟", 206, 94, C_DIM);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_TEXT, C_CARD);
  tft.drawString("07:30", 206, 112, 4);
  cnDraw(cn15, "周一至周五", 206, 144, C_DIM);
}

void drawFooter() {
  if (s_hasBg) dimBand(0, 168, 320, 40);
  tft.drawFastHLine(8, 170, 304, C_LINE);
  const char* hint = "单击停止  长按懒睡  双击夜灯";
  textS(cn18, hint, (320 - cnWidth(cn18, hint)) / 2, 176, C_DIM);
}

}  // namespace

// ---------- public ----------
void begin() {
  tft.init();
  tft.setRotation(TFT_ROTATION);
  Serial.printf("[ui] %d x %d\n", tft.width(), tft.height());

  s_hasBg = drawBgImg();
  if (!s_hasBg) tft.fillScreen(C_BG);

  s_timeW   = tft.textWidth("00:00:00", 7) + 8;
  s_timeBox = timeBox.init(s_timeW, kTimeH);
  Serial.printf("[ui] clock box %s, background %s\n",
                s_timeBox ? "ok" : "off", s_hasBg ? "photo" : "none");

  renderAll();
}

void renderAll() {
  if (s_hasBg) {
    if (!drawBgImg()) { s_hasBg = false; tft.fillScreen(C_BG); }
  } else {
    tft.fillScreen(C_BG);
  }
  drawHeader();
  if (s_hasBg) dimBand(0, 26, 320, 54);   // keep the digits readable over a photo
  paintTime();
  tft.fillRect(10, 82, 300, 3, C_LINE);
  s_lastBarW = 0;
  paintBar();
  drawWeatherCard();
  drawAlarmCard();
  drawFooter();
}

void setBackground() { s_hasBg = true; renderAll(); }

void loop() { /* icons are static now */ }

void tick(const struct tm& t) {
  s_timeOk = true;
  s_h = t.tm_hour; s_m = t.tm_min; s_s = t.tm_sec;

  static const char* wd[7] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
  char nd[32];
  snprintf(nd, sizeof(nd), "%s %d月%d日", wd[t.tm_wday], t.tm_mon + 1, t.tm_mday);
  if (strcmp(nd, s_date) != 0) {
    strncpy(s_date, nd, sizeof(s_date) - 1);
    s_date[sizeof(s_date) - 1] = 0;
    drawHeader();
  }
  paintTime();
  paintBar();
}

void setWifi(bool up, int rssi) { s_wifi = up; s_rssi = rssi; drawBars(); }

void setWeather(int tempC, const char* cond) { s_temp = tempC; s_cond = cond; drawWeatherCard(); }

void setAqi(int aqi, const char* label) { s_aqi = aqi; s_aqiText = label; drawWeatherCard(); }

void setCity(const char* name) {
  strncpy(s_city, name, sizeof(s_city) - 1);
  s_city[sizeof(s_city) - 1] = 0;
  drawWeatherCard();
}

}  // namespace ui
