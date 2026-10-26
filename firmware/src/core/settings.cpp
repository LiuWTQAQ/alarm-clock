#include "settings.h"
#include <Preferences.h>

namespace settings {
namespace {

const City kCities[] = {
  {"北京",     39.9042f, 116.4074f},
  {"上海",     31.2304f, 121.4737f},
  {"广州",     23.1291f, 113.2644f},
  {"深圳",     22.5431f, 114.0579f},
  {"杭州",     30.2741f, 120.1551f},
  {"成都",     30.5728f, 104.0668f},
  {"武汉",     30.5928f, 114.3055f},
  {"西安",     34.3416f, 108.9398f},
  {"南京",     32.0603f, 118.7969f},
  {"重庆",     29.5630f, 106.5516f},
  {"天津",     39.3434f, 117.3616f},
  {"苏州",     31.2989f, 120.5853f},
  {"长沙",     28.2282f, 112.9388f},
  {"青岛",     36.0671f, 120.3826f},
  {"大连",     38.9140f, 121.6147f},
  {"厦门",     24.4798f, 118.0894f},
  {"合肥",     31.8206f, 117.2272f},
  {"福州",     26.0745f, 119.2965f},
  {"昆明",     25.0389f, 102.7183f},
  {"南昌",     28.6820f, 115.8579f},
  {"贵阳",     26.6470f, 106.6302f},
  {"太原",     37.8706f, 112.5489f},
  {"石家庄",   38.0428f, 114.5149f},
  {"哈尔滨",   45.8038f, 126.5350f},
  {"长春",     43.8171f, 125.3235f},
  {"沈阳",     41.8057f, 123.4315f},
  {"郑州",     34.7466f, 113.6254f},
  {"济南",     36.6512f, 117.1201f},
  {"南宁",     22.8170f, 108.3665f},
  {"海口",     20.0442f, 110.1999f},
  {"三亚",     18.2528f, 109.5119f},
  {"兰州",     36.0611f, 103.8343f},
  {"银川",     38.4872f, 106.2309f},
  {"西宁",     36.6171f, 101.7782f},
  {"乌鲁木齐", 43.8256f,  87.6168f},
  {"呼和浩特", 40.8424f, 111.7490f},
  {"拉萨",     29.6520f,  91.1721f},
  {"香港",     22.3193f, 114.1694f},
  {"澳门",     22.1987f, 113.5439f},
  {"台北",     25.0330f, 121.5654f},
};

const int    kN = (int)(sizeof(kCities) / sizeof(kCities[0]));
Preferences  prefs;
int          s_idx = 0;
bool         s_hour12 = false;
uint8_t      s_lang   = 0;
bool         s_fahr   = false;

}  // namespace

void begin() {
  prefs.begin("cfg", true);
  int i = prefs.getInt("city", 5);   // 成都
  s_hour12 = prefs.getBool("h12", false);
  s_lang   = prefs.getUChar("lang", 0);
  s_fahr   = prefs.getBool("fahr", false);
  prefs.end();
  s_idx = (i >= 0 && i < kN) ? i : 0;
}

int         cityCount()       { return kN; }
const City& cityAt(int i)     { return kCities[(i >= 0 && i < kN) ? i : 0]; }
int         cityIndex()       { return s_idx; }

void setCityIndex(int i) {
  if (i < 0 || i >= kN) return;
  s_idx = i;
  prefs.begin("cfg", false);
  prefs.putInt("city", i);
  prefs.end();
}

const char* cityName()        { return kCities[s_idx].name; }

bool hour12()          { return s_hour12; }
void setHour12(bool v) {
  s_hour12 = v;
  prefs.begin("cfg", false); prefs.putBool("h12", v); prefs.end();
}
uint8_t lang()         { return s_lang; }

bool fahrenheit()      { return s_fahr; }
void setFahrenheit(bool v) {
  s_fahr = v;
  prefs.begin("cfg", false); prefs.putBool("fahr", v); prefs.end();
}
void setLang(uint8_t v) {
  s_lang = v ? 1 : 0;
  prefs.begin("cfg", false); prefs.putUChar("lang", s_lang); prefs.end();
}
float       lat()             { return kCities[s_idx].lat; }
float       lon()             { return kCities[s_idx].lon; }

}  // namespace settings
