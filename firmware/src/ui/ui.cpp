#include "ui.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <math.h>
#include <string.h>
#include "ui_font_cn.h"
#include "../core/alarms.h"
#include "../net/weather.h"

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
const uint16_t C_OK     = 0x07E0;

// ---------- small off-screen box (differential push) ----------
struct Box {
  TFT_eSprite spr;
  uint16_t*   prev = nullptr;
  int         w = 0, h = 0;

  explicit Box(TFT_eSPI* t) : spr(t) {}

  bool init(int w_, int h_) {
    w = w_; h = h_;
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
        } else c++;
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
int      s_wday   = 1;
int      s_temp   = -1;
const char* s_cond = "获取中";
char     s_date[32] = "---";
char     s_city[16] = "北京";
bool     s_wifi   = false;
int      s_rssi   = 0;
int      s_aqi    = -1;
const char* s_aqiText = "";

bool s_acOn = false;
bool s_lampOn = false;

enum Page : uint8_t { P_HOME, P_WEATHER, P_ALARMS, P_EDIT, P_CTRL };
Page s_page = P_HOME;
int  s_edit = -1;

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

inline uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {
  if (a == 0)  return bg;
  if (a == 15) return fg;
  int rb = (bg >> 11) & 0x1F, gb = (bg >> 5) & 0x3F, bb = bg & 0x1F;
  int rf = (fg >> 11) & 0x1F, gf = (fg >> 5) & 0x3F, bf = fg & 0x1F;
  int r = rb + ((rf - rb) * a) / 15;
  int g = gb + ((gf - gb) * a) / 15;
  int b = bb + ((bf - bb) * a) / 15;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// 4-bit antialiased blit. `base` is the colour already on screen behind the
// text; every coverage level is blended against it, which is what makes the
// edges smooth instead of stair-stepped.
void cnDrawTo(TFT_eSPI& dst, const CnFont& f, const char* s, int x, int yTop,
              uint16_t color, uint16_t base) {
  const char* p = s;
  int pen = x;
  while (*p) {
    const CnGlyph* g = cnLookup(f, utf8Next(p));
    if (!g) { pen += f.ascent / 2; continue; }
    if (g->w && g->h) {
      int ox = pen + g->xoff, oy = yTop + g->yoff;
      int rb = (g->w + 1) / 2;
      for (int row = 0; row < g->h; row++) {
        const uint8_t* line = f.data + g->off + row * rb;
        for (int c = 0; c < g->w; c++) {
          uint8_t b = line[c >> 1];
          uint8_t a = (c & 1) ? (b & 0x0F) : (b >> 4);
          if (!a) continue;
          dst.drawPixel(ox + c, oy + row, blend565(base, color, a));
        }
      }
    }
    pen += g->adv;
  }
}

void cnDraw(const CnFont& f, const char* s, int x, int yTop, uint16_t color, uint16_t base = C_BG) {
  cnDrawTo(tft, f, s, x, yTop, color, base);
}

void textS(const CnFont& f, const char* s, int x, int y, uint16_t color, uint16_t base = C_BG) {
  if (s_hasBg) cnDraw(f, s, x + 1, y + 1, TFT_BLACK, base);
  cnDraw(f, s, x, y, color, base);
}

void textC(const CnFont& f, const char* s, int cx, int yTop, uint16_t color, uint16_t base = C_BG) {
  cnDraw(f, s, cx - cnWidth(f, s) / 2, yTop, color, base);
}

void dimBand(int x, int y, int w, int h) {
  if (!s_hasBg) return;
  for (int j = 0; j < h; j++)
    for (int i = (j & 1); i < w; i += 2)
      tft.drawPixel(x + i, y + j, TFT_BLACK);
}

bool inR(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void pill(int x, int y, int w, int h, const char* label,
          uint16_t bg, uint16_t fg, const CnFont& f) {
  tft.fillRoundRect(x, y, w, h, 6, bg);
  cnDraw(f, label, x + (w - cnWidth(f, label)) / 2, y + (h - f.ascent) / 2 + 1, fg, bg);
}

// ---------- icons ----------
uint8_t kindOf(const char* s) {
  if (strstr(s, "雷"))                          return 5;
  if (strstr(s, "雪"))                          return 3;
  if (strstr(s, "雾"))                          return 4;
  if (strstr(s, "霾") || strstr(s, "沙尘"))     return 6;
  if (strstr(s, "雨"))                          return 2;
  if (strcmp(s, "晴") == 0)                     return 0;
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

void cloudShape(TFT_eSPI& g, int cx, int cy, uint16_t col) {
  g.fillCircle(cx - 8, cy, 8, col);
  g.fillCircle(cx + 2, cy - 5, 10, col);
  g.fillCircle(cx + 11, cy + 2, 7, col);
  g.fillRect  (cx - 8, cy, 20, 8, col);
}

void drawWeatherIcon(TFT_eSPI& g, int cx, int cy, uint8_t kind, int r = 1) {
  switch (kind) {
  case 0:
    for (int a = 0; a < 8; a++) {
      float q = a * 3.14159265f / 4.0f;
      g.drawLine(cx + (int)(cosf(q) * 13 * r), cy + (int)(sinf(q) * 13 * r),
                 cx + (int)(cosf(q) * 19 * r), cy + (int)(sinf(q) * 19 * r), C_WARM);
    }
    g.fillCircle(cx, cy, 9 * r, C_WARM);
    break;
  case 1:
    cloudShape(g, cx, cy, C_TEXT);
    break;
  case 2:
    cloudShape(g, cx, cy - 6, C_TEXT);
    for (int i = -1; i <= 1; i++) g.drawLine(cx + i * 8, cy + 8, cx + i * 8 - 3, cy + 15, C_ACCENT);
    break;
  case 3:
    cloudShape(g, cx, cy - 6, C_TEXT);
    for (int i = -1; i <= 1; i++) {
      g.drawFastHLine(cx + i * 9 - 3, cy + 12, 7, C_SNOW);
      g.drawFastVLine(cx + i * 9, cy + 9, 7, C_SNOW);
    }
    break;
  case 4:
    for (int i = 0; i < 3; i++) g.drawFastHLine(cx - 16, cy - 8 + i * 8, 32, C_DIM);
    break;
  case 5:
    cloudShape(g, cx, cy - 8, C_TEXT);
    g.drawLine(cx + 3, cy + 4,  cx - 2, cy + 13, C_WARM);
    g.drawLine(cx - 2, cy + 13, cx + 4, cy + 13, C_WARM);
    g.drawLine(cx + 4, cy + 13, cx,     cy + 21, C_WARM);
    break;
  default:
    cloudShape(g, cx, cy - 4, 0x9CD3);
    for (int i = 0; i < 4; i++) g.fillCircle(cx - 12 + i * 8, cy + 10 + (i & 1) * 4, 1, C_SAND);
    break;
  }
}

void bellShape(TFT_eSPI& g, int x, int y) {
  g.fillRoundRect(x - 9, y - 8, 18, 15, 4, C_WARM);
  g.fillRect     (x - 11, y + 7, 22, 3, C_WARM);
  g.fillCircle   (x, y + 12, 3, C_WARM);
}

void acShape(TFT_eSPI& g, int cx, int cy) {
  g.drawRoundRect(cx - 22, cy - 12, 44, 22, 5, C_ACCENT);
  for (int i = -1; i <= 1; i++) g.drawFastVLine(cx + i * 10, cy - 6, 10, C_ACCENT);
  g.fillRect(cx - 18, cy + 12, 36, 3, C_ACCENT);
}

void lampShape(TFT_eSPI& g, int cx, int cy) {
  g.fillCircle(cx, cy - 4, 9, C_WARM);
  g.fillRect(cx - 5, cy + 4, 10, 3, C_WARM);
  g.fillRect(cx - 3, cy + 8, 6, 2, C_WARM);
}

// ---------- background photo ----------
bool jpgCb(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
  if (y >= (int)tft.height()) return 0;
  for (int i = 0; i < (int)w * (int)h; i++) bmp[i] = (bmp[i] >> 1) & 0x7BEF;
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
  return TJpgDec.drawFsJpg(0, 0, "/bg.jpg", LittleFS) == JDR_OK;
}

// ---------- shared chrome ----------
void drawBars() {
  int bars = 0;
  if (s_wifi) bars = (s_rssi > -55) ? 4 : (s_rssi > -65) ? 3 : (s_rssi > -75) ? 2 : 1;
  if (bars == s_bars) return;
  s_bars = bars;
  if (s_hasBg) dimBand(226, 2, 26, 22);
  for (int i = 0; i < 4; i++)
    tft.fillRect(228 + i * 5, 17 - i * 3, 3, 4 + i * 3, (i < bars) ? C_ACCENT : C_LINE);
}

void topBar(const char* title) {
  tft.fillRect(0, 0, 320, 26, C_BG);
  tft.drawFastHLine(0, 26, 320, C_LINE);
  cnDraw(cn15, "返回", 8, 5, C_ACCENT);
  textC(cn18, title, 160, 3, C_TEXT);
}

// ---------- home ----------
void drawHomeHeader() {
  if (s_hasBg) dimBand(0, 0, 320, 25);
  else         tft.fillRect(0, 0, 320, 25, C_BG);
  tft.drawFastHLine(0, 24, 320, C_LINE);
  textS(cn18, s_date, 10, 2, C_DIM);

  bool anyOn = false;
  for (int i = 0; i < alarms::count(); i++)
    if (alarms::at(i)->on) anyOn = true;
  const char* st = anyOn ? "闹钟已开" : "闹钟已关";
  textS(cn15, st, 314 - cnWidth(cn15, st), 5, anyOn ? C_WARM : C_DIM);
  s_bars = -1;
  drawBars();
}

void paintTime() {
  char buf[16];
  if (!s_timeOk) snprintf(buf, sizeof(buf), "--:--:--");
  else           snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s_h, s_m, s_s);

  int tw = cnWidth(cn64, buf);
  int w  = tw + 8;
  if (w > 316) w = 316;

  if (s_hasBg || !s_timeBox) {
    if (s_hasBg) {
      tft.fillRect(0, kTimeY, 320, kTimeH, C_BG);
      dimBand(0, kTimeY, 320, kTimeH);
    }
    textC(cn64, buf, 160, kTimeY, C_TEXT);
    return;
  }
  if (!timeBox.spr.created()) {
    timeBox.init(w, kTimeH);
    s_timeW = w;
  }
  timeBox.clear(C_BG);
  int x = (s_timeW - tw) / 2;
  cnDrawTo(timeBox.spr, cn64, buf, x < 0 ? 0 : x, 0, C_TEXT, C_BG);
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
  drawWeatherIcon(tft, 32, 126, kindOf(s_cond));

  cnDraw(cn15, s_city, 64, 92, C_DIM, C_CARD);

  char t[8];
  if (s_temp < 0) snprintf(t, sizeof(t), "--");
  else            snprintf(t, sizeof(t), "%d", s_temp);
  cnDraw(cn28, t, 64, 107, C_TEXT, C_CARD);

  int x = 64 + cnWidth(cn28, t) + 2;
  cnDraw(cn15, "°C", x, 120, C_WARM, C_CARD);
  x += cnWidth(cn15, "°C") + 4;
  cnDraw(cn15, s_cond, x, 121, C_DIM, C_CARD);

  x = 64;
  cnDraw(cn15, "空气", x, 142, C_DIM, C_CARD);
  x += cnWidth(cn15, "空气") + 4;
  char q[8];
  if (s_aqi < 0) snprintf(q, sizeof(q), "--");
  else           snprintf(q, sizeof(q), "%d", s_aqi);
  cnDraw(cn15, q, x, 142, C_TEXT, C_CARD);
  x += cnWidth(cn15, q) + 4;
  if (s_aqi >= 0) cnDraw(cn15, s_aqiText, x, 142, aqiColor(s_aqi), C_CARD);
}

void drawAlarmCard() {
  tft.fillRoundRect(166, 90, 146, 74, 8, C_CARD);
  bellShape(tft, 184, 124);

  cnDraw(cn15, "下一个闹钟", 206, 94, C_DIM, C_CARD);

  alarms::Alarm a;
  if (alarms::next(s_h, s_m, s_wday, a)) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", a.hour, a.min);
    cnDraw(cn28, buf, 206, 106, C_TEXT, C_CARD);
    char rep[16];
    cnDraw(cn15, alarms::repeatText(a.mask, rep, sizeof(rep)), 206, 144, C_DIM, C_CARD);
  } else {
    cnDraw(cn18, "未设置", 206, 112, C_DIM, C_CARD);
  }
}

void drawCtrlRow() {
  // two entry tiles: air conditioner and lamp
  tft.fillRoundRect(8, 170, 148, 34, 8, C_CARD);
  acShape(tft, 26, 186);
  cnDraw(cn15, "空调", 46, 178, C_TEXT, C_CARD);

  tft.fillRoundRect(164, 170, 148, 34, 8, C_CARD);
  lampShape(tft, 182, 186);
  cnDraw(cn15, "灯", 202, 178, C_TEXT, C_CARD);
}

void drawHomeFooter() {
  const char* hint = "单击停止  长按懒睡  双击夜灯";
  textS(cn15, hint, (320 - cnWidth(cn15, hint)) / 2, 212, C_DIM);
}

void drawHome() {
  if (s_hasBg) { if (!drawBgImg()) { s_hasBg = false; tft.fillScreen(C_BG); } }
  else         tft.fillScreen(C_BG);

  drawHomeHeader();
  if (s_hasBg) dimBand(0, 26, 320, 54);
  paintTime();
  tft.fillRect(10, 82, 300, 3, C_LINE);
  s_lastBarW = 0;
  paintBar();
  drawWeatherCard();
  drawAlarmCard();
  drawCtrlRow();
  drawHomeFooter();
}

// ---------- weather detail ----------
void drawWeatherPage() {
  tft.fillScreen(C_BG);
  topBar("天气详情");

  drawWeatherIcon(tft, 44, 58, kindOf(s_cond));

  char t[8];
  if (s_temp < 0) snprintf(t, sizeof(t), "--");
  else            snprintf(t, sizeof(t), "%d", s_temp);
  cnDraw(cn28, t, 86, 36, C_TEXT);
  int x = 86 + cnWidth(cn28, t) + 3;
  cnDraw(cn15, "°C", x, 50, C_WARM);
  cnDraw(cn18, s_cond, 86, 66, C_DIM);

  char line[32];
  snprintf(line, sizeof(line), "体感 %d°C", weather::apparentC());
  cnDraw(cn15, line, 86, 88, C_DIM);
  snprintf(line, sizeof(line), "湿度 %d%%", weather::humidity());
  cnDraw(cn15, line, 200, 88, C_DIM);

  tft.drawFastHLine(8, 108, 304, C_LINE);
  cnDraw(cn15, "未来三天", 10, 112, C_ACCENT);

  static const char* names[3] = {"明天", "后天", "大后天"};
  for (int i = 0; i < 3; i++) {
    int y = 130 + i * 30;
    cnDraw(cn15, names[i], 12, y + 2, C_DIM);
    drawWeatherIcon(tft, 92, y + 8, kindOf(weather::textForCode(weather::dayCode(i))), 0.6f);

    char hi[8], lo[8];
    snprintf(hi, sizeof(hi), "%d", weather::dayMax(i));
    snprintf(lo, sizeof(lo), "%d", weather::dayMin(i));
    cnDraw(cn18, hi, 140, y, C_WARM);
    cnDraw(cn15, "/", 140 + cnWidth(cn18, hi) + 6, y + 3, C_DIM);
    cnDraw(cn18, lo, 140 + cnWidth(cn18, hi) + 18, y, C_ACCENT);
  }

  textC(cn15, "点击空白处返回", 160, 216, C_DIM);
}

// ---------- alarm list ----------
void drawAlarmList() {
  tft.fillScreen(C_BG);
  topBar("闹钟");

  int n = alarms::count();
  if (n == 0) textC(cn18, "未设置闹钟", 160, 100, C_DIM);

  for (int i = 0; i < n; i++) {
    alarms::Alarm* a = alarms::at(i);
    int y = 32 + i * 30;
    if (y + 28 > 196) break;

    tft.fillRoundRect(8, y, 304, 28, 6, C_CARD);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", a->hour, a->min);
    cnDraw(cn28, buf, 16, y - 1, a->on ? C_TEXT : C_DIM, C_CARD);

    char rep[16];
    cnDraw(cn15, alarms::repeatText(a->mask, rep, sizeof(rep)), 118, y + 7,
           a->on ? C_DIM : C_LINE, C_CARD);
    pill(250, y + 3, 54, 22, a->on ? "开" : "关",
         a->on ? 0x0320 : 0x2104, a->on ? C_OK : C_DIM, cn15);
  }

  if (n < alarms::kMax) pill(8, 206, 304, 28, "添加闹钟", C_CARD, C_ACCENT, cn15);
}

// ---------- alarm edit ----------
void drawEdit() {
  alarms::Alarm* a = alarms::at(s_edit);
  if (!a) return;

  tft.fillScreen(C_BG);
  topBar("编辑闹钟");

  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", a->hour, a->min);
  cnDraw(cn64, buf, 160 - cnWidth(cn64, buf) / 2, 36, C_TEXT, C_BG);

  pill(8,   96, 72, 30, "时-", C_CARD, C_TEXT, cn15);
  pill(88,  96, 72, 30, "时+", C_CARD, C_TEXT, cn15);
  pill(168, 96, 72, 30, "分-", C_CARD, C_TEXT, cn15);
  pill(248, 96, 72, 30, "分+", C_CARD, C_TEXT, cn15);

  char rep[16];
  char line[24];
  snprintf(line, sizeof(line), "重复  %s", alarms::repeatText(a->mask, rep, sizeof(rep)));
  pill(8, 136, 304, 30, line, C_CARD, C_TEXT, cn15);

  pill(8, 174, 304, 30, a->on ? "闹钟已开" : "闹钟已关",
       a->on ? 0x0320 : 0x2104, a->on ? C_OK : C_DIM, cn15);

  pill(8,   208, 136, 26, "删除", 0x3000, C_TEXT, cn15);
  pill(176, 208, 136, 26, "保存", 0x0320, C_TEXT, cn15);
}

// ---------- smart home ----------
void drawCtrl() {
  tft.fillScreen(C_BG);
  topBar("智能家居");

  tft.fillRoundRect(8, 44, 304, 72, 8, C_CARD);
  acShape(tft, 34, 76);
  cnDraw(cn18, "空调", 64, 54, C_TEXT, C_CARD);
  cnDraw(cn15, "红外遥控  房间空调", 64, 82, C_DIM, C_CARD);
  cnDraw(cn15, s_acOn ? "开" : "关", 268, 66, s_acOn ? C_OK : C_DIM, C_CARD);

  tft.fillRoundRect(8, 128, 304, 72, 8, C_CARD);
  lampShape(tft, 34, 162);
  cnDraw(cn18, "灯", 64, 138, C_TEXT, C_CARD);
  cnDraw(cn15, "继电器  卧室顶灯", 64, 166, C_DIM, C_CARD);
  cnDraw(cn15, s_lampOn ? "开" : "关", 268, 150, s_lampOn ? C_OK : C_DIM, C_CARD);

  textC(cn15, "点击卡片切换开关", 160, 212, C_DIM);
}

void drawCurrent() {
  switch (s_page) {
  case P_HOME:    drawHome();      break;
  case P_WEATHER: drawWeatherPage(); break;
  case P_ALARMS:  drawAlarmList(); break;
  case P_EDIT:    drawEdit();      break;
  case P_CTRL:    drawCtrl();      break;
  }
}

void go(Page p) {
  Serial.printf("[ui] page %d\n", (int)p);
  s_page = p;
  drawCurrent();
}

}  // namespace

// ---------- public ----------
void begin() {
  tft.init();
  tft.setRotation(TFT_ROTATION);
  Serial.printf("[ui] %d x %d\n", tft.width(), tft.height());

  s_hasBg = drawBgImg();
  if (!s_hasBg) tft.fillScreen(C_BG);

  s_timeW   = cnWidth(cn64, "00:00:00") + 8;
  s_timeBox = timeBox.init(s_timeW, kTimeH);
  Serial.printf("[ui] clock box %s (w %d), background %s\n",
                s_timeBox ? "ok" : "off", s_timeW, s_hasBg ? "photo" : "none");

  drawCurrent();
}

void renderAll() { drawCurrent(); }

void clearBackground() {
  LittleFS.remove("/bg.jpg");
  s_hasBg = false;
  s_page  = P_HOME;
  drawCurrent();
  Serial.println("[ui] background cleared");
}

void setBackground() {
  s_hasBg  = true;
  s_page   = P_HOME;
  drawCurrent();
}

void loop() { /* pages are event driven; nothing runs per frame */ }

void tick(const struct tm& t) {
  s_timeOk = true;
  s_h = t.tm_hour; s_m = t.tm_min; s_s = t.tm_sec; s_wday = t.tm_wday;
  if (s_page != P_HOME) return;      // do not repaint a list while the user reads it

  static const char* wd[7] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
  char nd[32];
  snprintf(nd, sizeof(nd), "%s %d月%d日", wd[t.tm_wday], t.tm_mon + 1, t.tm_mday);
  if (strcmp(nd, s_date) != 0) {
    strncpy(s_date, nd, sizeof(s_date) - 1);
    s_date[sizeof(s_date) - 1] = 0;
    drawHomeHeader();
  }
  paintTime();
  paintBar();
}

void setWifi(bool up, int rssi) {
  s_wifi = up; s_rssi = rssi;
  if (s_page == P_HOME) drawBars();
}

void setWeather(int tempC, const char* cond) {
  s_temp = tempC; s_cond = cond;
  if (s_page == P_HOME)        drawWeatherCard();
  else if (s_page == P_WEATHER) drawWeatherPage();
}

void setAqi(int aqi, const char* label) {
  s_aqi = aqi; s_aqiText = label;
  if (s_page == P_HOME) drawWeatherCard();
}

void setCity(const char* name) {
  strncpy(s_city, name, sizeof(s_city) - 1);
  s_city[sizeof(s_city) - 1] = 0;
  if (s_page == P_HOME) drawWeatherCard();
}

void back() {
  if (s_page == P_HOME) return;
  s_page = P_HOME;
  drawHome();
}

void tap(int x, int y) {
  switch (s_page) {
  case P_HOME:
    if (inR(x, y, 8, 90, 150, 74))   { go(P_WEATHER); return; }
    if (inR(x, y, 166, 90, 146, 74)) { go(P_ALARMS);  return; }
    if (inR(x, y, 8, 170, 148, 34))  { go(P_CTRL);    return; }
    if (inR(x, y, 164, 170, 148, 34)){ go(P_CTRL);    return; }
    break;

  case P_WEATHER:
    back();
    break;

  case P_ALARMS: {
    for (int i = 0; i < alarms::count(); i++) {
      int yy = 32 + i * 30;
      if (inR(x, y, 8, yy, 304, 28)) {
        if (x > 246) {                       // the pill toggles
          alarms::Alarm* a = alarms::at(i);
          a->on = !a->on;
          alarms::save();
          drawAlarmList();
        } else {
          s_edit = i;
          go(P_EDIT);
        }
        return;
      }
    }
    if (y > 200 && x > 4) {
      if (alarms::add() >= 0) { s_edit = alarms::count() - 1; go(P_EDIT); }
    }
    break;
  }

  case P_EDIT: {
    alarms::Alarm* a = alarms::at(s_edit);
    if (!a) { go(P_ALARMS); return; }
    if (inR(x, y, 8, 96, 72, 30))   { a->hour = (a->hour + 23) % 24; alarms::save(); drawEdit(); return; }
    if (inR(x, y, 88, 96, 72, 30))  { a->hour = (a->hour + 1) % 24;  alarms::save(); drawEdit(); return; }
    if (inR(x, y, 168, 96, 72, 30)) { a->min = (a->min + 59) % 60;   alarms::save(); drawEdit(); return; }
    if (inR(x, y, 248, 96, 72, 30)) { a->min = (a->min + 1) % 60;    alarms::save(); drawEdit(); return; }
    if (inR(x, y, 8, 136, 304, 30)) {                        // repeat cycles
      a->mask = (a->mask == 0) ? 0x7F : (a->mask == 0x7F) ? 0x1F
              : (a->mask == 0x1F) ? 0x60 : 0;
      alarms::save(); drawEdit(); return;
    }
    if (inR(x, y, 8, 174, 304, 30)) { a->on = !a->on; alarms::save(); drawEdit(); return; }
    if (inR(x, y, 8, 208, 136, 26))  { alarms::removeAt(s_edit); s_edit = -1; go(P_ALARMS); return; }
    if (inR(x, y, 176, 208, 136, 26)){ alarms::save(); s_edit = -1; go(P_ALARMS); return; }
    break;
  }

  case P_CTRL:
    if (inR(x, y, 8, 44, 304, 72))  { s_acOn = !s_acOn;   drawCtrl(); return; }
    if (inR(x, y, 8, 128, 304, 72)) { s_lampOn = !s_lampOn; drawCtrl(); return; }
    break;
  }

  if (y < 30 && x < 70) back();
}

}  // namespace ui
