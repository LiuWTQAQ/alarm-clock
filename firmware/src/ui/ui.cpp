// ui: home / weather / alarms / edit / smart-home / settings
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
#include "config.h"
#include "settings.h"

namespace ui {
namespace {

TFT_eSPI tft;

const uint16_t C_BG     = TFT_BLACK;
const uint16_t C_TEXT   = 0xFFFF;
const uint16_t C_DIM    = 0x8C71;
const uint16_t C_ACCENT = 0x07FF;
const uint16_t C_WARM   = 0xFD20;
const uint16_t C_CARD   = 0x18E3;
const uint16_t C_LINE   = 0x3186;
const uint16_t C_SNOW   = 0xBDF7;
const uint16_t C_OK     = 0x07E0;
const uint16_t C_SAND   = 0xC5C0;

// ---------- i18n ----------
enum Str : uint8_t {
  S_HOME, S_SETTINGS, S_DEVICES, S_AC, S_LAMP, S_AC_SUB, S_LAMP_SUB,
  S_ON, S_OFF, S_WEATHER, S_FEELS, S_HUMID, S_NEXT3, S_DAY1, S_DAY2, S_DAY3,
  S_TAP_BACK, S_ALARMS, S_ADD_ALARM, S_NO_ALARM, S_ONCE, S_DAILY, S_WEEKDAY,
  S_WEEKEND, S_CUSTOM, S_SAVE, S_DELETE, S_REPEAT, S_EDIT_ALARM, S_HR_DEC,
  S_HR_INC, S_MIN_DEC, S_MIN_INC, S_ALARM_ON, S_ALARM_OFF, S_TAP_TOGGLE,
  S_TIME_FMT, S_H24, S_H12, S_LANG, S_WX_UPDATED, S_BACK, S_AIR, S_SMART,
  S_TAP_CARD, S_NOT_SET, S_NEVER, S_NEXT_ALARM, S_TEMP_UNIT, S_WORLD, S_LONDON,
  S_NEWYORK, S_BEIJING, S_CELSIUS, S_FAHRENHEIT, S_COUNT
};

const char* kZh[S_COUNT] = {
  "主页", "设置", "家居", "空调", "灯", "红外遥控  房间空调", "继电器  卧室顶灯",
  "开", "关", "天气详情", "体感", "湿度", "未来三天", "明天", "后天", "大后天",
  "点击空白处返回", "闹钟", "添加闹钟", "未设置闹钟", "一次", "每天", "工作日",
  "周末", "自定义", "保存", "删除", "重复", "编辑闹钟", "时-",
  "时+", "分-", "分+", "闹钟已开", "闹钟已关", "点击卡片切换开关",
  "时间格式", "24 小时制", "12 小时制", "系统语言", "天气更新", "返回", "空气",
  "智能家居", "点击卡片切换开关", "未设置", "从未更新", "下一个闹钟", "温度单位", "世界时间", "伦敦", "纽约", "北京", "摄氏度", "华氏度",
};

const char* kEn[S_COUNT] = {
  "Home", "Settings", "Devices", "AC", "Lamp", "IR remote - Bedroom AC", "Relay - Bedroom lamp",
  "On", "Off", "Weather", "Feels", "Humidity", "Next 3 days", "Tomorrow", "Day after", "In 3 days",
  "Tap to go back", "Alarms", "Add alarm", "No alarms", "Once", "Daily", "Weekdays",
  "Weekend", "Custom", "Save", "Delete", "Repeat", "Edit alarm", "Hr-",
  "Hr+", "Min-", "Min+", "Alarm on", "Alarm off", "Tap a card to toggle",
  "Time format", "24-hour", "12-hour", "Language", "Weather updated", "Back", "Air",
  "Smart home", "Tap a card to toggle", "Not set", "Never", "Next alarm", "Temperature", "World clock", "London", "New York", "Beijing",
  "Celsius", "Fahrenheit",
};

const char* T(Str s) { return (settings::lang() == 0 ? kZh : kEn)[s]; }

const char* condText(const char* zh) {
  if (settings::lang() == 0) return zh;
  struct M { const char* zh; const char* en; };
  static const M m[] = {
    {"晴", "Clear"}, {"多云", "Cloudy"}, {"阴", "Overcast"}, {"晴间多云", "Partly cloudy"},
    {"雾", "Fog"}, {"霾", "Haze"}, {"小雨", "Light rain"}, {"中雨", "Rain"},
    {"大雨", "Heavy rain"}, {"暴雨", "Storm"}, {"雷雨", "Thunder"}, {"雷阵雨", "Thunderstorm"},
    {"阵雨", "Showers"}, {"小雪", "Light snow"}, {"中雪", "Snow"}, {"大雪", "Heavy snow"},
    {"阵雪", "Snow showers"}, {"冻雨", "Freezing rain"}, {"雪", "Snow"}, {"雨", "Rain"},
    {"沙尘暴", "Sandstorm"}, {"浮尘", "Dust"}, {"扬沙", "Sand"},
  };
  for (const M& e : m) if (strcmp(e.zh, zh) == 0) return e.en;
  return zh;
}

// ---------- small off-screen box, pushed in one SPI transaction ----------
struct Box {
  TFT_eSprite spr;
  int w = 0, h = 0;
  explicit Box(TFT_eSPI* t) : spr(t) {}
  bool init(int w_, int h_) {
    w = w_; h = h_;
    spr.setColorDepth(16);
    return spr.createSprite(w, h) != nullptr;
  }
  void clear(uint16_t col) { spr.fillSprite(col); }
  void push(int x, int y) {
    tft.startWrite();
    tft.pushImage(x, y, w, h, (uint16_t*)spr.getPointer());
    tft.endWrite();
  }
};

Box clockBox(&tft);
bool s_clockBox = false;
int  s_clockW = 0;
int  s_bars = -1;
bool s_hasBg = false;

int      s_h = 0, s_m = 0, s_s = 0;
bool     s_timeOk = false;
int      s_wday = 1;
int      s_dow = 1, s_mon = 1, s_mday = 1, s_year = 2026;
int      s_temp = -1;
const char* s_cond = "获取中";
char     s_date[32] = "---";
char     s_city[16] = "北京";
bool     s_wifi = false;
int      s_rssi = 0;
int      s_aqi = -1;
const char* s_aqiText = "";
bool     s_acOn = false;
bool     s_lampOn = false;

enum Page : uint8_t { P_HOME, P_WEATHER, P_ALARMS, P_EDIT, P_CTRL, P_SETTINGS, P_WORLD };
Page s_page = P_HOME;
int  s_edit = -1;

// ---------- utf8 / cjk ----------
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

void pill(int x, int y, int w, int h, const char* label, uint16_t bg, uint16_t fg,
          const CnFont& f) {
  tft.fillRoundRect(x, y, w, h, 6, bg);
  cnDraw(f, label, x + (w - cnWidth(f, label)) / 2, y + (h - f.ascent) / 2 + 1, fg, bg);
}

bool inR(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void dimBand(int x, int y, int w, int h) {
  if (!s_hasBg) return;
  for (int j = 0; j < h; j++)
    for (int i = (j & 1); i < w; i += 2)
      tft.drawPixel(x + i, y + j, TFT_BLACK);
}

// ---------- time formatting ----------
void rebuildDate() {
  static const char* zh[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
  static const char* en[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  if (settings::lang() == 0)
    snprintf(s_date, sizeof(s_date), "%s %d月%d日", zh[s_dow], s_mon, s_mday);
  else
    snprintf(s_date, sizeof(s_date), "%s %d/%d", en[s_dow], s_mon, s_mday);
}

void fmtHM(char* b, size_t n, int h, int m) {
  if (settings::hour12()) {
    int hh = h % 12; if (hh == 0) hh = 12;
    snprintf(b, n, "%d:%02d", hh, m);
  } else {
    snprintf(b, n, "%02d:%02d", h, m);
  }
}

void fmtHMS(char* b, size_t n, int h, int m, int s) {
  if (settings::hour12()) {
    int hh = h % 12; if (hh == 0) hh = 12;
    snprintf(b, n, "%d:%02d:%02d", hh, m, s);
  } else {
    snprintf(b, n, "%02d:%02d:%02d", h, m, s);
  }
}

const char* ampm(int h) { return h < 12 ? "AM" : "PM"; }

// ---------- icons ----------
void iconSun(int cx, int cy, int r) {
  for (int a = 0; a < 8; a++) {
    float ang = a * 3.14159265f / 4.0f;
    float c = cosf(ang), sn = sinf(ang);
    tft.drawLine(cx + (int)(r * 1.05f * c), cy + (int)(r * 1.05f * sn),
                 cx + (int)(r * 1.45f * c), cy + (int)(r * 1.45f * sn), C_WARM);
  }
  tft.fillCircle(cx, cy, (int)(r * 0.62f), C_WARM);
}

void iconCloud(int cx, int cy, int r) {
  tft.fillCircle(cx - (int)(r * 0.55f), cy + (int)(r * 0.15f), (int)(r * 0.55f), C_TEXT);
  tft.fillCircle(cx + (int)(r * 0.10f), cy - (int)(r * 0.25f), (int)(r * 0.70f), C_TEXT);
  tft.fillCircle(cx + (int)(r * 0.60f), cy + (int)(r * 0.10f), (int)(r * 0.50f), C_TEXT);
  tft.fillRoundRect(cx - (int)(r * 0.95f), cy + (int)(r * 0.10f), (int)(r * 1.9f), (int)(r * 0.60f),
                    (int)(r * 0.30f), C_TEXT);
}

void iconRain(int cx, int cy, int r) {
  tft.fillCircle(cx - (int)(r * 0.55f), cy - (int)(r * 0.20f), (int)(r * 0.50f), C_TEXT);
  tft.fillCircle(cx + (int)(r * 0.10f), cy - (int)(r * 0.55f), (int)(r * 0.62f), C_TEXT);
  tft.fillCircle(cx + (int)(r * 0.60f), cy - (int)(r * 0.25f), (int)(r * 0.45f), C_TEXT);
  tft.fillRoundRect(cx - (int)(r * 0.95f), cy - (int)(r * 0.30f), (int)(r * 1.9f), (int)(r * 0.55f),
                    (int)(r * 0.27f), C_TEXT);
  for (int i = 0; i < 3; i++) {
    int x = cx - (int)(r * 0.55f) + i * (int)(r * 0.55f);
    tft.drawLine(x, (int)(cy + r * 0.45f), x - 2, (int)(cy + r * 0.95f), C_ACCENT);
  }
}

void iconSnow(int cx, int cy, int r) {
  tft.fillCircle(cx - (int)(r * 0.55f), cy - (int)(r * 0.20f), (int)(r * 0.50f), C_TEXT);
  tft.fillCircle(cx + (int)(r * 0.10f), cy - (int)(r * 0.55f), (int)(r * 0.62f), C_TEXT);
  tft.fillCircle(cx + (int)(r * 0.60f), cy - (int)(r * 0.25f), (int)(r * 0.45f), C_TEXT);
  tft.fillRoundRect(cx - (int)(r * 0.95f), cy - (int)(r * 0.30f), (int)(r * 1.9f), (int)(r * 0.55f),
                    (int)(r * 0.27f), C_TEXT);
  for (int i = 0; i < 3; i++) {
    int x = cx - (int)(r * 0.55f) + i * (int)(r * 0.60f);
    int y = cy + (int)(r * 0.75f);
    tft.drawFastHLine(x - 3, y, 7, C_SNOW);
    tft.drawFastVLine(x, y - 3, 7, C_SNOW);
  }
}

void iconFog(int cx, int cy, int r) {
  tft.fillCircle(cx - (int)(r * 0.45f), cy - (int)(r * 0.35f), (int)(r * 0.45f), C_DIM);
  tft.fillCircle(cx + (int)(r * 0.15f), cy - (int)(r * 0.60f), (int)(r * 0.55f), C_DIM);
  tft.fillRoundRect(cx - (int)(r * 0.85f), cy - (int)(r * 0.45f), (int)(r * 1.7f), (int)(r * 0.50f),
                    (int)(r * 0.25f), C_DIM);
  for (int i = 0; i < 3; i++)
    tft.drawFastHLine(cx - r + i * 2, cy + (int)(r * 0.25f) + i * 5, 2 * r - i * 5, C_LINE);
}

void iconThunder(int cx, int cy, int r) {
  tft.fillCircle(cx - (int)(r * 0.55f), cy - (int)(r * 0.30f), (int)(r * 0.48f), C_TEXT);
  tft.fillCircle(cx + (int)(r * 0.10f), cy - (int)(r * 0.62f), (int)(r * 0.60f), C_TEXT);
  tft.fillCircle(cx + (int)(r * 0.60f), cy - (int)(r * 0.35f), (int)(r * 0.44f), C_TEXT);
  tft.fillRoundRect(cx - (int)(r * 0.95f), cy - (int)(r * 0.40f), (int)(r * 1.9f), (int)(r * 0.52f),
                    (int)(r * 0.26f), C_TEXT);
  tft.fillTriangle(cx + 2, (int)(cy + r * 0.15f), cx - 6, (int)(cy + r * 0.85f),
                   cx + 6, (int)(cy + r * 0.85f), C_WARM);
  tft.fillTriangle(cx + 1, (int)(cy + r * 0.55f), cx - 6, (int)(cy + r * 1.05f),
                   cx + 5, (int)(cy + r * 0.95f), C_WARM);
}

void iconHaze(int cx, int cy, int r) {
  tft.fillCircle(cx, cy - (int)(r * 0.35f), (int)(r * 0.55f), C_SAND);
  for (int i = 0; i < 3; i++)
    tft.drawFastHLine(cx - r, cy + (int)(r * 0.30f) + i * 5, 2 * r, C_LINE);
}

void drawWeatherIcon(int cx, int cy, int kind, int r) {
  switch (kind) {
    case 0: iconSun(cx, cy, r); break;
    case 1: iconCloud(cx, cy, r); break;
    case 2: iconRain(cx, cy, r); break;
    case 3: iconSnow(cx, cy, r); break;
    case 4: iconFog(cx, cy, r); break;
    case 5: iconThunder(cx, cy, r); break;
    default: iconHaze(cx, cy, r); break;
  }
}

const char* repText(uint8_t mask, char* b, size_t n) {
  if (settings::lang() == 0) return alarms::repeatText(mask, b, n);
  if (mask == 0x00) return "Once";
  if (mask == 0x7F) return "Daily";
  if (mask == 0x1F) return "Weekdays";
  if (mask == 0x60) return "Weekend";
  return "Custom";
}

uint8_t kindOf(const char* s) {
  if (!s) return 1;
  if (strstr(s, "雷")) return 5;
  if (strstr(s, "雪")) return 3;
  if (strstr(s, "雾")) return 4;
  if (strstr(s, "霾") || strstr(s, "尘") || strstr(s, "沙")) return 6;
  if (strstr(s, "雨")) return 2;
  if (strstr(s, "晴")) return (strstr(s, "云") || strstr(s, "阴")) ? 1 : 0;
  return 1;
}

void bellIcon(int x, int y, int r, uint16_t col) {
  tft.fillCircle(x, y - (int)(r * 0.15f), (int)(r * 0.62f), col);
  tft.fillRect(x - (int)(r * 0.62f), y - (int)(r * 0.15f), (int)(r * 1.24f), (int)(r * 0.62f), col);
  tft.fillRoundRect(x - (int)(r * 0.85f), y + (int)(r * 0.42f), (int)(r * 1.7f), (int)(r * 0.26f), 2, col);
  tft.fillCircle(x, y + (int)(r * 0.85f), (int)(r * 0.16f), col);
}

static const uint32_t kWorld[32][2] = {
  {0x00000000,0x00000000},
  {0x00000000,0x00000000},
  {0x00000000,0x00000000},
  {0x07FFE000,0x00004000},
  {0x3FFFF000,0x0000FF00},
  {0x07FFFC00,0x0F03FFF8},
  {0x00FFFC00,0x7E07FFF8},
  {0x007FFE00,0x7E3FFFFC},
  {0x007FF000,0xFFFFFFE0},
  {0x003FE003,0xF3FFFF80},
  {0x001FE000,0x3CFFFE00},
  {0x000FC000,0x7E3FFC00},
  {0x00020000,0xFE1FF000},
  {0x0000000F,0xFF0FE000},
  {0x00007807,0xFFC78000},
  {0x00007F03,0xFF800000},
  {0x00007F80,0x7F000000},
  {0x00007FC0,0x7E000000},
  {0x00007FC0,0x3E000380},
  {0x00003FC0,0x3C000FE0},
  {0x00003F00,0x3C000FF0},
  {0x00003E00,0x18000FE0},
  {0x00003C00,0x10000840},
  {0x00003800,0x00000000},
  {0x00003800,0x00000000},
  {0x00001000,0x00000000},
  {0x00000000,0x00000000},
  {0x00000000,0x00000000},
  {0x00000000,0xFFFFFFFF},
  {0x00000000,0xFFFFFFFF},
  {0x00000000,0xFFFFFFFF},
  {0x00000000,0xFFFFFFFF},
};

// ---------- background photo ----------
extern const unsigned char bg_jpg[];
extern const unsigned int bg_jpg_len;

bool jpgPush(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
  if (y >= 240) return false;
  if (y + h > 240) h = 240 - y;
  if (x + w > 320) w = 320 - x;
  tft.pushImage(x, y, w, h, bmp);
  return true;
}

bool drawBgImg() {
  if (!LittleFS.exists("/bg.jpg")) return false;
  File f = LittleFS.open("/bg.jpg", "r");
  if (!f) return false;
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(jpgPush);
  TJpgDec.drawFsJpg(0, 0, f);
  f.close();
  dimBand(0, 0, 320, 240);
  return true;
}

// ---------- home pieces ----------
void drawTopLine() {
  tft.fillRect(0, 0, 320, 22, C_BG);
  tft.drawFastHLine(0, 22, 320, C_LINE);
}

void drawHeader() {
  tft.fillRect(0, 0, 320, 22, C_BG);
  textS(cn18, s_date, 10, 1, C_DIM);

  int x = 226;
  for (int i = 0; i < 4; i++) {
    int h = 3 + i * 2;
    bool on = s_wifi && (s_rssi > -40 - i * 12);
    tft.fillRect(x + i * 4, 18 - h, 3, h, on ? C_ACCENT : C_LINE);
  }
  bool anyOn = false;
  for (int i = 0; i < alarms::count(); i++) if (alarms::at(i)->on) anyOn = true;
  const char* st = settings::lang() ? (anyOn ? "Alarm on" : "Alarm off")
                                    : (anyOn ? "闹钟已开" : "闹钟已关");
  textS(cn15, st, 314 - cnWidth(cn15, st), 3, anyOn ? C_WARM : C_DIM);
  tft.drawFastHLine(0, 22, 320, C_LINE);
}

void drawClock() {
  char buf[20];
  if (s_timeOk) fmtHMS(buf, sizeof(buf), s_h, s_m, s_s);
  else          snprintf(buf, sizeof(buf), "--:--:--");

  bool h12 = settings::hour12();
  const char* ap = h12 ? ampm(s_h) : "";
  int aw = h12 ? cnWidth(cn15, ap) + 8 : 0;
  int tw = cnWidth(cn64, buf);
  int w  = aw + tw + 8;
  if (w > 316) w = 316;

  if (!s_clockBox || w != s_clockW) {
    if (s_clockBox) { clockBox.spr.deleteSprite(); s_clockBox = false; }
    s_clockBox = clockBox.init(w, 58);
    s_clockW = w;
    Serial.printf("[ui] clock box %s (%d px)\n", s_clockBox ? "ok" : "off", w);
    if (!s_clockBox) {
      textC(cn64, buf, 160, 24, C_TEXT);
      return;
    }
  }
  clockBox.clear(C_BG);
  cnDrawTo(clockBox.spr, cn64, buf, aw + 4, 0, C_TEXT, C_BG);
  if (h12) cnDrawTo(clockBox.spr, cn15, ap, 5, 16, C_WARM, C_BG);
  clockBox.push(160 - s_clockW / 2, 24);
}

void drawBar() {
  tft.fillRect(10, 84, 300, 3, C_LINE);
  int w = s_timeOk ? (s_s * 300) / 59 : 0;
  if (w) tft.fillRect(10, 84, w, 3, C_ACCENT);
}

void drawWeatherCard() {
  tft.fillRoundRect(8, 90, 150, 74, 8, C_CARD);
  drawWeatherIcon(30, 120, kindOf(s_cond), 11);

  cnDraw(cn15, s_city, 60, 93, C_DIM, C_CARD);

  char t[12];
  int  v = s_temp;
  if (s_temp > -90 && settings::fahrenheit()) v = (s_temp * 9 + 2) / 5 + 32;
  if (v > -90) snprintf(t, sizeof(t), "%d", v); else snprintf(t, sizeof(t), "--");
  cnDraw(cn28, t, 60, 108, C_TEXT, C_CARD);

  int x = 60 + cnWidth(cn28, t) + 4;
  tft.drawCircle(x + 3, 114, 3, C_WARM);
  tft.drawCircle(x + 3, 114, 2, C_WARM);
  cnDraw(cn15, settings::fahrenheit() ? "F" : "C", x + 8, 121, C_WARM, C_CARD);
  x += 8 + cnWidth(cn15, "C") + 5;
  cnDraw(cn15, condText(s_cond), x, 121, C_DIM, C_CARD);

  x = 60;
  cnDraw(cn15, T(S_AIR), x, 143, C_DIM, C_CARD);
  x += cnWidth(cn15, T(S_AIR)) + 4;
  char q[8];
  if (s_aqi >= 0) snprintf(q, sizeof(q), "%d", s_aqi); else snprintf(q, sizeof(q), "--");
  cnDraw(cn15, q, x, 143, C_TEXT, C_CARD);
  x += cnWidth(cn15, q) + 4;
  if (s_aqi >= 0) cnDraw(cn15, s_aqiText, x, 143, C_DIM, C_CARD);
}

void drawAlarmCard() {
  tft.fillRoundRect(166, 90, 146, 74, 8, C_CARD);
  bellIcon(186, 121, 11, C_WARM);
  cnDraw(cn15, T(S_NEXT_ALARM), 206, 92, C_DIM, C_CARD);

  alarms::Alarm a;
  if (alarms::next(s_h, s_m, 2000 + s_year, s_mon, s_mday, s_wday, a)) {
    char buf[12];
    fmtHM(buf, sizeof(buf), a.hour, a.min);
    cnDraw(cn28, buf, 206, 110, C_TEXT, C_CARD);
    if (settings::hour12()) {
      int x = 206 + cnWidth(cn28, buf) + 2;
      cnDraw(cn15, ampm(a.hour), x, 122, C_WARM, C_CARD);
    }
    char rep[16];
    cnDraw(cn15, repText(a.mask, rep, sizeof(rep)), 206, 146, C_DIM, C_CARD);
  } else {
    cnDraw(cn18, T(S_NO_ALARM), 206, 112, C_DIM, C_CARD);
  }
}

void homeIcon(int x, int y, int kind) {
  if (kind == 0) {
    tft.drawCircle(x, y - 2, 7, C_ACCENT);
    tft.drawFastHLine(x - 7, y + 3, 15, C_ACCENT);
    for (int i = -2; i <= 2; i++) tft.drawFastVLine(x + i * 3, y + 6, 4, C_ACCENT);
  } else {
    tft.drawCircle(x, y - 1, 6, C_DIM);
    tft.drawCircle(x, y - 1, 2, C_DIM);
    for (int a = 0; a < 6; a++) {
      float ang = a * 3.14159265f / 3.0f;
      tft.drawLine(x + (int)(7 * cosf(ang)), y - 1 + (int)(7 * sinf(ang)),
                   x + (int)(10 * cosf(ang)), y - 1 + (int)(10 * sinf(ang)), C_DIM);
    }
  }
}

void drawTiles() {
  tft.fillRoundRect(8, 170, 148, 34, 6, C_CARD);
  homeIcon(24, 187, 0);
  cnDraw(cn15, T(S_DEVICES), 40, 177, C_TEXT, C_CARD);

  tft.fillRoundRect(164, 170, 148, 34, 6, C_CARD);
  homeIcon(180, 187, 1);
  cnDraw(cn15, T(S_SETTINGS), 196, 177, C_TEXT, C_CARD);
}

void drawHomeHints() {
  const char* hint = settings::lang()
      ? "1x stop    hold snooze    2x lamp"
      : "单击停止  长按懒睡  双击夜灯";
  textS(cn15, hint, (320 - cnWidth(cn15, hint)) / 2, 212, C_DIM);
}

void drawHome() {
  if (s_hasBg) drawBgImg(); else tft.fillScreen(C_BG);
  rebuildDate();
  drawHeader();
  drawClock();
  drawBar();
  drawWeatherCard();
  drawAlarmCard();
  drawTiles();
  drawHomeHints();
}

// ---------- page chrome ----------
void topBar(const char* title) {
  tft.fillRect(0, 0, 320, 27, C_BG);
  tft.drawFastHLine(0, 27, 320, C_LINE);
  cnDraw(cn15, T(S_BACK), 8, 6, C_ACCENT);
  textC(cn18, title, 160, 4, C_TEXT);
}

void drawWeatherPage() {
  if (s_hasBg) drawBgImg(); else tft.fillScreen(C_BG);
  topBar(T(S_WEATHER));

  uint8_t kind = kindOf(s_cond);
  int ix = 40, iy = 62;
  switch (kind) {
    case 0: iconSun(ix, iy, 12); break;
    case 1: iconCloud(ix, iy, 12); break;
    case 2: iconRain(ix, iy, 12); break;
    case 3: iconSnow(ix, iy, 12); break;
    case 4: iconFog(ix, iy, 12); break;
    case 5: iconThunder(ix, iy, 12); break;
    default: iconHaze(ix, iy, 12); break;
  }
  char t[12];
  if (s_temp > -90) snprintf(t, sizeof(t), "%d", s_temp); else snprintf(t, sizeof(t), "--");
  cnDraw(cn28, t, 84, 34, C_TEXT);
  int x = 84 + cnWidth(cn28, t) + 2;
  cnDraw(cn15, "°C", x, 47, C_WARM);
  cnDraw(cn18, condText(s_cond), 84, 66, C_DIM);

  char line[40];
  if (settings::lang()) {
    snprintf(line, sizeof(line), "%s %d°C   %s %d%%", T(S_FEELS), weather::apparentC(),
             T(S_HUMID), weather::humidity());
  } else {
    snprintf(line, sizeof(line), "%s %d°C   %s %d%%", T(S_FEELS), weather::apparentC(),
             T(S_HUMID), weather::humidity());
  }
  cnDraw(cn15, line, 84, 90, C_DIM);

  tft.drawFastHLine(16, 112, 288, C_LINE);
  cnDraw(cn15, T(S_NEXT3), 10, 118, C_ACCENT);

  for (int i = 0; i < 3; i++) {
    int y = 142 + i * 30;
    const char* lab = T(i == 0 ? S_DAY1 : i == 1 ? S_DAY2 : S_DAY3);
    cnDraw(cn15, lab, 12, y + 2, C_DIM);
    int code = weather::dayCode(i);
    uint8_t k = kindOf(weather::textForCode(code));
    switch (k) {
      case 0: iconSun(56, y + 10, 6); break;
      case 1: iconCloud(56, y + 10, 6); break;
      case 2: iconRain(56, y + 10, 6); break;
      case 3: iconSnow(56, y + 10, 6); break;
      case 4: iconFog(56, y + 10, 6); break;
      case 5: iconThunder(56, y + 10, 6); break;
      default: iconHaze(56, y + 10, 6); break;
    }
    char hi[8], lo[8];
    if (weather::dayMax(i) > -90) snprintf(hi, sizeof(hi), "%d", weather::dayMax(i));
    else snprintf(hi, sizeof(hi), "--");
    if (weather::dayMin(i) > -90) snprintf(lo, sizeof(lo), "%d", weather::dayMin(i));
    else snprintf(lo, sizeof(lo), "--");
    cnDraw(cn18, hi, 96, y, C_WARM);
    cnDraw(cn15, "/", 96 + cnWidth(cn18, hi) + 6, y + 3, C_DIM);
    cnDraw(cn18, lo, 96 + cnWidth(cn18, hi) + 18, y, C_ACCENT);
  }
  textC(cn15, T(S_TAP_BACK), 160, 216, C_DIM);
}

void drawAlarmPage() {
  if (s_hasBg) drawBgImg(); else tft.fillScreen(C_BG);
  topBar(T(S_ALARMS));

  int n = alarms::count();
  if (n == 0) textC(cn18, T(S_NO_ALARM), 160, 100, C_DIM);

  for (int i = 0; i < n; i++) {
    alarms::Alarm* a = alarms::at(i);
    int y = 32 + i * 30;
    if (y + 28 > 196) break;

    tft.fillRoundRect(8, y, 304, 28, 6, C_CARD);
    char buf[12];
    fmtHM(buf, sizeof(buf), a->hour, a->min);
    cnDraw(cn28, buf, 16, y - 1, a->on ? C_TEXT : C_DIM, C_CARD);
    if (settings::hour12()) {
      int x = 16 + cnWidth(cn28, buf) + 2;
      cnDraw(cn15, ampm(a->hour), x, y + 6, C_WARM, C_CARD);
    }

    char rep[16];
    cnDraw(cn15, repText(a->mask, rep, sizeof(rep)), 150, y + 7,
           a->on ? C_DIM : C_LINE, C_CARD);
    pill(250, y + 3, 54, 22, a->on ? T(S_ON) : T(S_OFF),
         a->on ? 0x0320 : 0x2104, a->on ? C_TEXT : C_DIM, cn15);
  }

  if (n < alarms::kMax) {
    pill(8, 208, 304, 30, T(S_ADD_ALARM), 0x0A2A, C_ACCENT, cn15);
  }
}

void drawEditPage() {
  if (s_hasBg) drawBgImg(); else tft.fillScreen(C_BG);
  topBar(T(S_EDIT_ALARM));

  alarms::Alarm* a = alarms::at(s_edit);
  if (!a) { back(); return; }

  char buf[12];
  fmtHM(buf, sizeof(buf), a->hour, a->min);
  textC(cn64, buf, 160, 30, C_TEXT);

  pill(8,   90, 72, 30, T(S_HR_DEC), C_CARD, C_TEXT, cn15);
  pill(88,  90, 72, 30, T(S_HR_INC), C_CARD, C_TEXT, cn15);
  pill(168, 90, 72, 30, T(S_MIN_DEC), C_CARD, C_TEXT, cn15);
  pill(248, 90, 64, 30, T(S_MIN_INC), C_CARD, C_TEXT, cn15);

  char rep[16];
  tft.fillRoundRect(8, 126, 304, 30, 6, C_CARD);
  cnDraw(cn15, T(S_REPEAT), 20, 133, C_DIM, C_CARD);
  cnDraw(cn15, repText(a->mask, rep, sizeof(rep)), 90, 133, C_TEXT, C_CARD);

  pill(8, 162, 304, 30, a->on ? T(S_ALARM_ON) : T(S_ALARM_OFF),
       a->on ? 0x0320 : C_CARD, C_TEXT, cn15);

  pill(8,   206, 96, 28, T(S_DELETE), 0x6000, C_TEXT, cn15);
  pill(176, 206, 136, 28, T(S_SAVE), 0x0300, C_TEXT, cn15);
}

void drawCtrlPage() {
  if (s_hasBg) drawBgImg(); else tft.fillScreen(C_BG);
  topBar(T(S_SMART));

  tft.fillRoundRect(8, 36, 304, 74, 8, C_CARD);
  cnDraw(cn18, T(S_AC), 62, 46, C_TEXT, C_CARD);
  cnDraw(cn15, T(S_AC_SUB), 62, 76, C_DIM, C_CARD);
  cnDraw(cn15, s_acOn ? T(S_ON) : T(S_OFF), 288 - cnWidth(cn15, T(S_ON)), 56,
         s_acOn ? C_OK : C_DIM, C_CARD);
  for (int a = 0; a < 6; a++) {
    float ang = a * 3.14159265f / 3.0f;
    tft.drawLine(30 + (int)(9 * cosf(ang)), 73 + (int)(9 * sinf(ang)),
                 30 + (int)(14 * cosf(ang)), 73 + (int)(14 * sinf(ang)), C_ACCENT);
  }
  tft.drawCircle(30, 73, 4, C_ACCENT);

  tft.fillRoundRect(8, 120, 304, 74, 8, C_CARD);
  cnDraw(cn18, T(S_LAMP), 62, 130, C_TEXT, C_CARD);
  cnDraw(cn15, T(S_LAMP_SUB), 62, 160, C_DIM, C_CARD);
  cnDraw(cn15, s_lampOn ? T(S_ON) : T(S_OFF), 288 - cnWidth(cn15, T(S_ON)), 140,
         s_lampOn ? C_OK : C_DIM, C_CARD);
  tft.fillCircle(30, 157, 9, s_lampOn ? C_WARM : C_LINE);
  tft.fillRect(27, 166, 7, 5, C_LINE);

  textC(cn15, T(S_TAP_CARD), 160, 214, C_DIM);
}

void drawSettingsPage() {
  if (s_hasBg) drawBgImg(); else tft.fillScreen(C_BG);
  topBar(T(S_SETTINGS));

  cnDraw(cn15, T(S_TIME_FMT), 12, 42, C_DIM);
  pill(140, 34, 168, 30, settings::hour12() ? T(S_H12) : T(S_H24), C_CARD, C_ACCENT, cn15);

  cnDraw(cn15, T(S_LANG), 12, 86, C_DIM);
  pill(140, 78, 168, 30, settings::lang() ? "English" : "中文", C_CARD, C_ACCENT, cn15);

  cnDraw(cn15, T(S_TEMP_UNIT), 12, 130, C_DIM);
  pill(140, 122, 168, 30, settings::fahrenheit() ? T(S_FAHRENHEIT) : T(S_CELSIUS), C_CARD, C_ACCENT, cn15);

  cnDraw(cn15, T(S_WX_UPDATED), 12, 176, C_DIM);
  char upd[24];
  unsigned long up = weather::lastUpdate();     // millis() of the last fetch
  if (up > 0 && time(nullptr) > 1600000000) {
    time_t lu = time(nullptr) - (time_t)((millis() - up) / 1000);
    struct tm tmv;
    localtime_r(&lu, &tmv);
    snprintf(upd, sizeof(upd), "%04d-%02d-%02d %02d:%02d:%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
  } else {
    snprintf(upd, sizeof(upd), "--");
  }
  cnDraw(cn15, upd, 140, 176, C_TEXT);

  cnDraw(cn15, settings::lang() ? "Tap to change" : "点击切换", 12, 212, C_DIM);
}

// ---------- dispatch ----------

// ---------- world clock ----------
const int kMapX = 0, kMapY = 132, kMapW = 320, kMapH = 96;

void cityNow(int idx, struct tm& out) {
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  int off = 8;                                   // Beijing
  if (idx == 0) {                                // London: BST last Sun Mar .. last Sun Oct
    bool bst = (lt.tm_mon > 2 && lt.tm_mon < 9) ||
               (lt.tm_mon == 2 && lt.tm_mday >= 25) ||
               (lt.tm_mon == 9 && lt.tm_mday < 25);
    off = bst ? 1 : 0;
  }
  if (idx == 1) {                                // New York: EDT 2nd Sun Mar .. 1st Sun Nov
    bool edt = (lt.tm_mon > 2 && lt.tm_mon < 10) ||
               (lt.tm_mon == 2 && lt.tm_mday >= 8) ||
               (lt.tm_mon == 10 && lt.tm_mday < 1);
    off = edt ? -4 : -5;
  }
  time_t t = now + off * 3600;
  gmtime_r(&t, &out);
}

void drawWorldMap() {
  const float DEG = 3.14159265f / 180.0f;
  time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  int doy = utc.tm_yday;
  float decl = 23.44f * sinf(2.0f * 3.14159265f * (284 + doy) / 365.0f);
  float subsolar = -15.0f * (utc.tm_hour + utc.tm_min / 60.0f - 12.0f);

  tft.fillRect(kMapX, kMapY, kMapW, kMapH, 0x0841);
  // land
  for (int gy = 0; gy < 32; gy++) {
    uint32_t hi = kWorld[gy][0], lo = kWorld[gy][1];
    if (!hi && !lo) continue;
    for (int gx = 0; gx < 64; gx++) {
      bool land = gx < 32 ? (hi & (1u << (31 - gx))) : (lo & (1u << (63 - gx)));
      if (land) tft.fillRect(kMapX + gx * 5, kMapY + gy * 3, 5, 3, 0x4A69);
    }
  }
  // graticule
  for (int i = 1; i < 6; i++)
    tft.drawFastVLine(kMapX + i * kMapW / 6, kMapY, kMapH, 0x18E3);
  for (int i = 1; i < 4; i++)
    tft.drawFastHLine(kMapX, kMapY + i * kMapH / 4, kMapW, 0x18E3);

  // night shading (checkerboard) + terminator curve
  float sinLat[96], cosLat[96], cosH[320];
  for (int y = 0; y < kMapH; y++) {
    float lat = (90.0f - (y + 0.5f) * 180.0f / kMapH) * DEG;
    sinLat[y] = sinf(lat); cosLat[y] = cosf(lat);
  }
  for (int x = 0; x < kMapW; x++) {
    float lon = -180.0f + (x + 0.5f) * 360.0f / kMapW;
    cosH[x] = cosf((lon - subsolar) * DEG);
  }
  float sd = sinf(decl * DEG), cd = cosf(decl * DEG);
  for (int y = 0; y < kMapH; y++)
    for (int x = 0; x < kMapW; x++) {
      float e = sinLat[y] * sd + cosLat[y] * cd * cosH[x];
      if (e < 0 && ((x + y) & 1)) tft.drawPixel(kMapX + x, kMapY + y, 0x1082);
    }
  if (fabsf(tanf(decl * DEG)) > 0.02f) {
    for (int x = 0; x < kMapW; x++) {
      float phi = atanf(-cosH[x] / tanf(decl * DEG)) / DEG;
      int y = (int)((90.0f - phi) * kMapH / 180.0f);
      if (y >= 0 && y < kMapH) tft.drawPixel(kMapX + x, kMapY + y, C_WARM);
    }
  }
  // city markers
  const float clat[3] = {51.5f, 40.7f, 39.9f};
  const float clon[3] = {-0.13f, -74.0f, 116.4f};
  for (int i = 0; i < 3; i++) {
    int x = kMapX + (int)((clon[i] + 180.0f) * kMapW / 360.0f);
    int y = kMapY + (int)((90.0f - clat[i]) * kMapH / 180.0f);
    tft.fillCircle(x, y, 2, C_ACCENT);
  }
}

void drawWorldTimes() {
  for (int i = 0; i < 3; i++) {
    int y = 32 + i * 30;
    tft.fillRect(150, y, 166, 26, s_hasBg ? C_BG : C_BG);
    struct tm t;
    cityNow(i, t);
    char b[12];
    fmtHMS(b, sizeof(b), t.tm_hour, t.tm_min, t.tm_sec);
    cnDraw(cn28, b, 306 - cnWidth(cn28, b), y - 2, C_TEXT, C_BG);
  }
}

void drawWorldPage() {
  if (s_hasBg) drawBgImg(); else tft.fillScreen(C_BG);
  topBar(T(S_WORLD));
  const Str names[3] = {S_LONDON, S_NEWYORK, S_BEIJING};
  for (int i = 0; i < 3; i++) {
    int y = 32 + i * 30;
    tft.fillRoundRect(8, y, 304, 28, 6, C_CARD);
    cnDraw(cn15, T(names[i]), 16, y + 6, C_DIM, C_CARD);
  }
  drawWorldTimes();
  drawWorldMap();
  cnDraw(cn15, settings::lang() ? "tap to go back" : "点击空白处返回", 12, 232, C_DIM);
}

void drawCurrent() {
  switch (s_page) {
    case P_HOME:     drawHome();        break;
    case P_WEATHER:  drawWeatherPage(); break;
    case P_ALARMS:   drawAlarmPage();   break;
    case P_EDIT:     drawEditPage();    break;
    case P_CTRL:     drawCtrlPage();    break;
    case P_SETTINGS: drawSettingsPage();break;
    case P_WORLD:    drawWorldPage();    break;
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
  tft.setRotation(3);
  Serial.printf("[ui] %d x %d\n", tft.width(), tft.height());

  s_hasBg = drawBgImg();
  if (!s_hasBg) tft.fillScreen(C_BG);

  drawCurrent();
}

void loop() {}

void renderAll() { drawCurrent(); }

void clearBackground() {
  LittleFS.remove("/bg.jpg");
  s_hasBg = false;
  s_page  = P_HOME;
  drawCurrent();
  Serial.println("[ui] background cleared");
}

void setBackground() {
  bool had = s_hasBg;
  s_hasBg = drawBgImg();
  if (!s_hasBg && had) tft.fillScreen(C_BG);
  drawCurrent();
  Serial.printf("[ui] background %s\n", s_hasBg ? "photo" : "none");
}

void tick(const struct tm& t) {
  bool wasOk = s_timeOk;
  bool dateChanged = false;
  static int lastWday = -1, lastDay = -1;

  s_h = t.tm_hour; s_m = t.tm_min; s_s = t.tm_sec;
  s_wday = t.tm_wday;
  s_timeOk = true;

  if (t.tm_wday != lastWday || t.tm_mday != lastDay) {
    lastWday = t.tm_wday; lastDay = t.tm_mday;
    s_dow = t.tm_wday; s_mon = t.tm_mon + 1; s_mday = t.tm_mday; s_year = t.tm_year + 1900;
    rebuildDate();
    dateChanged = true;
  }

  if (s_page == P_WORLD) { drawWorldTimes(); return; }
  if (s_page != P_HOME) return;
  if (!wasOk || dateChanged) drawHeader();
  drawClock();
  drawBar();
}

void setWifi(bool up, int rssi) {
  if (up == s_wifi && rssi == s_rssi && s_bars >= 0) return;
  s_wifi = up; s_rssi = rssi;
  s_bars = 1;
  if (s_page == P_HOME) drawHeader();
}

void setCity(const char* name) {
  snprintf(s_city, sizeof(s_city), "%s", name);
  if (s_page == P_HOME) drawWeatherCard();
}

void setWeather(int tempC, const char* cond) {
  s_temp = tempC;
  s_cond = cond;
  if (s_page == P_HOME) drawWeatherCard();
  if (s_page == P_WEATHER) drawWeatherPage();
}

void setAqi(int aqi, const char* label) {
  s_aqi = aqi;
  s_aqiText = label;
  if (s_page == P_HOME) drawWeatherCard();
}

void tap(int x, int y) {
  switch (s_page) {
  case P_HOME:
    if (inR(x, y, 0, 24, 320, 56))   { go(P_WORLD);    return; }
    if (inR(x, y, 8, 90, 150, 74))   { go(P_WEATHER);  return; }
    if (inR(x, y, 166, 90, 146, 74)) { go(P_ALARMS);   return; }
    if (inR(x, y, 8, 170, 148, 34))  { go(P_CTRL);     return; }
    if (inR(x, y, 164, 170, 148, 34)){ go(P_SETTINGS); return; }
    break;

  case P_WEATHER:
    if (y < 27 || y > 206) { go(P_HOME); return; }
    break;
  case P_WORLD:
    if (y < 27 || y > 224) { go(P_HOME); return; }
    break;

  case P_ALARMS: {
    if (y < 27) { go(P_HOME); return; }
    int n = alarms::count();
    for (int i = 0; i < n; i++) {
      int yy = 32 + i * 30;
      if (!inR(x, y, 8, yy, 304, 28)) continue;
      if (inR(x, y, 250, yy + 3, 54, 22)) {
        alarms::Alarm* a = alarms::at(i);
        a->on = !a->on;
        alarms::save();
        drawAlarmPage();
        drawHeader();
      } else {
        s_edit = i;
        go(P_EDIT);
      }
      return;
    }
    if (n < alarms::kMax && inR(x, y, 8, 208, 304, 30)) {
      int i = alarms::add();
      if (i >= 0) { s_edit = i; go(P_EDIT); }
    }
    break;
  }

  case P_EDIT: {
    alarms::Alarm* a = alarms::at(s_edit);
    if (!a) { go(P_ALARMS); return; }
    if (y < 27) { alarms::save(); go(P_ALARMS); return; }
    if (inR(x, y, 8,   90, 72, 30)) { a->hour = (a->hour + 23) % 24; alarms::save(); drawEditPage(); return; }
    if (inR(x, y, 88,  90, 72, 30)) { a->hour = (a->hour + 1) % 24;  alarms::save(); drawEditPage(); return; }
    if (inR(x, y, 168, 90, 72, 30)) { a->min = (a->min + 55) % 60;   alarms::save(); drawEditPage(); return; }
    if (inR(x, y, 248, 90, 64, 30)) { a->min = (a->min + 5) % 60;    alarms::save(); drawEditPage(); return; }
    if (inR(x, y, 8, 126, 304, 30)) {
      a->mask = (a->mask == 0x7F) ? 0x1F : (a->mask == 0x1F) ? 0x60 : (a->mask == 0x60) ? 0 : 0x7F;
      alarms::save();
      drawEditPage();
      return;
    }
    if (inR(x, y, 8, 162, 304, 30)) { a->on = !a->on; alarms::save(); drawEditPage(); return; }
    if (inR(x, y, 8, 206, 96, 28))  { alarms::removeAt(s_edit); go(P_ALARMS); return; }
    if (inR(x, y, 176, 206, 136, 28)) { alarms::save(); go(P_ALARMS); return; }
    break;
  }

  case P_CTRL:
    if (y < 27) { go(P_HOME); return; }
    if (inR(x, y, 8, 36, 304, 74))  { s_acOn = !s_acOn;     drawCtrlPage(); return; }
    if (inR(x, y, 8, 120, 304, 74)) { s_lampOn = !s_lampOn; drawCtrlPage(); return; }
    break;

  case P_SETTINGS:
    if (y < 27) { go(P_HOME); return; }
    if (inR(x, y, 12, 38, 296, 32)) {
      settings::setHour12(!settings::hour12());
      drawSettingsPage();
      return;
    }
    if (inR(x, y, 12, 88, 296, 32)) {
      settings::setLang(settings::lang() ? 0 : 1);
      drawSettingsPage();
      return;
    }
    if (inR(x, y, 12, 122, 296, 32)) {
      settings::setFahrenheit(!settings::fahrenheit());
      drawSettingsPage();
      return;
    }
    if (y > 200) { go(P_HOME); return; }
    break;
  }
}

void back() {
  switch (s_page) {
    case P_EDIT:     alarms::save(); go(P_ALARMS); break;
    case P_ALARMS:
    case P_WEATHER:
    case P_CTRL:
    case P_SETTINGS: go(P_HOME); break;
    default: break;
  }
}

void debugDumpClock() {
  if (!s_clockBox) { Serial.println("NOCLOCK"); return; }
  uint16_t* img = (uint16_t*)clockBox.spr.getPointer();
  Serial.printf("DUMP %d %d\n", s_clockW, 58);
  for (int i = 0; i < s_clockW * 58; i++) {
    Serial.printf("%04X", img[i]);
    if ((i & 31) == 31) Serial.println();
  }
  Serial.println("END");
}

void debugDumpFull() {
  uint16_t* buf = (uint16_t*)malloc(320 * 240 * 2);
  if (!buf) { Serial.println("NOMEM"); return; }
  tft.readRect(0, 0, 320, 240, buf);
  Serial.printf("FULL 320 240\n");
  for (int i = 0; i < 320 * 240; i++) {
    Serial.printf("%04X", buf[i]);
    if ((i & 31) == 31) Serial.print('\n');
  }
  Serial.println("END");
  free(buf);
}

void debugDumpScreen() {
  uint16_t* buf = (uint16_t*)malloc(320 * 100 * 2);
  if (!buf) { Serial.println("NOMEM"); return; }
  tft.readRect(0, 0, 320, 100, buf);
  Serial.printf("SCR 320 100\n");
  for (int i = 0; i < 320 * 100; i++) {
    Serial.printf("%04X", buf[i]);
    if ((i & 31) == 31) Serial.println();
  }
  Serial.println("END");
  free(buf);
}

}  // namespace ui
