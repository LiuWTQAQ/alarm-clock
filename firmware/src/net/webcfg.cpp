#include "webcfg.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include "settings.h"
#include "weather.h"
#include "net_time.h"
#include "../ui/ui.h"

namespace webcfg {
namespace {

WebServer server(80);
bool      s_started = false;
File      s_upload;
bool      s_uploadOk = false;

const char* HEAD = R"HTML(<!doctype html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>智能闹钟设置</title>
<style>
body{font-family:-apple-system,system-ui,"Microsoft YaHei",sans-serif;background:#0e1116;color:#e6e9ef;margin:0;padding:22px}
h1{font-size:19px;font-weight:600;margin:0 0 18px}
.card{background:#171c24;border-radius:14px;padding:18px;margin-bottom:14px}
label{font-size:13px;color:#8b96a5}
select,input[type=file]{width:100%;box-sizing:border-box;padding:11px;margin-top:8px;border-radius:10px;border:1px solid #273140;background:#0f1319;color:#fff;font-size:15px}
button{margin-top:14px;width:100%;padding:12px;border:0;border-radius:10px;background:#2f81f7;color:#fff;font-size:16px}
img{width:100%;border-radius:10px;margin-top:12px;background:#000}
.note{font-size:12px;color:#7d8797;margin-top:10px;line-height:1.6}
</style></head><body><h1>智能闹钟 · 设置</h1>
<form class="card" method="POST" action="/wifi"><label>Wi-Fi 名称（SSID）</label>
<input name="ssid" maxlength="32" value=")HTML";

const char* MIDW = R"HTML(">
<label style="display:block;margin-top:12px">Wi-Fi 密码</label>
<input name="pass" type="password" maxlength="64" placeholder="留空则保持原密码">
<button>保存并重连</button>
<div class="note">保存后设备会断开当前网络并连接新网络，约 5–10 秒；<br>如果连不上，用串口监视器可以看到重试日志。</div>
</form>
<form class="card" method="POST" action="/save"><label>天气城市</label><select name="city">)HTML";

const char* MID = R"HTML(</select><button>保存城市</button>
<div class="note">保存后立即按新城市拉取天气，约 3–5 秒。</div></form>
<form class="card" method="POST" action="/upload" enctype="multipart/form-data">
<label>背景图（JPEG，建议 320×240 或按比例）</label>
<input type="file" name="f" accept="image/jpeg">
<button>上传并应用</button>
<div class="note">图片会按等比缩放铺满屏幕，并自动压暗一半以保证文字可读。上传后立即生效。</div>
<img src="/bg.jpg" alt="当前背景">
<button formaction="/bg/clear" formmethod="get" style="background:#30363d">恢复默认背景</button></form>
</body></html>)HTML";

String page() {
  String p;
  p.reserve(4096);
  p += HEAD;
  p += net_time::ssid();
  p += MIDW;
  for (int i = 0; i < settings::cityCount(); i++) {
    p += "<option value=\"";
    p += i;
    p += "\"";
    if (i == settings::cityIndex()) p += " selected";
    p += ">";
    p += settings::cityAt(i).name;
    p += "</option>";
  }
  p += MID;
  return p;
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", page());
}

void handleSave() {
  int i = server.arg("city").toInt();
  settings::setCityIndex(i);
  ui::setCity(settings::cityName());
  weather::restart();
  Serial.printf("[cfg] city -> %s (%.3f, %.3f)\n",
                settings::cityName(), settings::lat(), settings::lon());
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleWifi() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  if (!ssid.length()) {
    server.send(400, "text/html; charset=utf-8",
                "<meta charset=utf-8><p>SSID 不能为空</p><p><a href=\"/\">返回</a></p>");
    return;
  }
  if (!pass.length()) pass = String(net_time::pass());
  net_time::setCredentials(ssid.c_str(), pass.c_str());
  server.send(200, "text/html; charset=utf-8",
              "<meta charset=utf-8><meta name=viewport content='width=device-width'>"
              "<body style='font-family:sans-serif;background:#0e1116;color:#e6e9ef;padding:24px'>"
              "<h3>已保存</h3><p>正在连接 <b>" + ssid + "</b>，约 5–10 秒。</p>"
              "<p>屏幕顶部会显示新网络名；若 30 秒后仍显示旧名或离线，请重新连接旧网络后改回。</p>"
              "</body>");
}

void handleUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    s_upload = LittleFS.open("/bg.jpg", "w");
    s_uploadOk = (bool)s_upload;
    Serial.printf("[cfg] upload start: %s\n", up.filename.c_str());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_upload) s_upload.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (s_upload) s_upload.close();
    Serial.printf("[cfg] upload end: %u bytes\n", (unsigned)up.totalSize);
    if (s_uploadOk) ui::setBackground();
  }
}

void handleBg() {
  File f = LittleFS.open("/bg.jpg", "r");
  if (!f) { server.send(404, "text/plain", "no background"); return; }
  server.streamFile(f, "image/jpeg");
  f.close();
}

}  // namespace

void begin() {
  LittleFS.begin(true);
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/wifi", HTTP_POST, handleWifi);
  server.on("/upload", HTTP_POST,
            []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); },
            handleUpload);
  server.on("/bg/clear", []() {
    ui::clearBackground();
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/bg.jpg", handleBg);
  server.begin();
  MDNS.begin("alarm");
  s_started = true;
  Serial.printf("[cfg] http://%s/  http://alarm.local/\n",
                WiFi.localIP().toString().c_str());
}

void loop() { server.handleClient(); }

bool started() { return s_started; }

}  // namespace webcfg
