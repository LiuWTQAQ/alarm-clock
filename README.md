# 基于 ESP32-S3 的云端互联智能闹钟与本地控制网关

> 本科毕业设计 · 西南交通大学—利兹学院 · 2026–2027
> Cloud-Connected Smart Alarm Clock and Local Control Gateway Based on ESP32-S3

一句话概括：**一台能远程设定、断网也能响的桌面闹钟，同时充当房间里的红外与灯光控制网关。**

---


## 显示与字体

- 字体：华文黑体（STHeiti Medium）离线生成 C 点阵，**4 bit/px 覆盖率**，运行时按底色做 alpha 混合，所以边缘是渐变的而不是锯齿（真正抗锯齿）。
- 三个字号 `cn28 / cn18 / cn15` 覆盖界面全部中文，`cn64` 只含数字与冒号，用来画时钟。
- 时钟走 254×56 的差分 sprite，宽度按实际字宽计算，避免大字号被裁切；只有像素变化的列才会推到屏幕上。

## 页面与交互

屏幕共四个页面，互为主从关系：

```
主页 ──点天气卡──▶ 天气详情 ──点空白/返回──▶ 主页
  ├──点闹钟卡──▶ 闹钟列表 ──点单条──▶ 编辑 ──保存/删除──▶ 列表
  └──点空调/灯──▶ 智能家居 ──点卡片切换开关──▶ 智能家居
```

| 页面 | 内容 | 触控热区（`ui::tap` 坐标） |
|------|------|---------------------------|
| 主页 | 日期、时钟、天气卡、下一个闹钟、空调/灯入口 | 天气卡 (8,90,150×74)、闹钟卡 (166,90,146×74)、空调 (8,170,148×34)、灯 (164,170,148×34) |
| 天气详情 | 当前天气、体感温度、湿度、未来三天高低温 | 顶部返回 (0,0,60×26)、底部任意处 (y>210) |
| 闹钟列表 | 至多 5 条，时间/重复/开关，点行进入编辑 | 行 (8,32+i×30,304×28)、开关 (250,32+i×30,54×22)、添加 (8,208) |
| 编辑 | 时/分加减、重复、启停、删除、保存 | 四个加减键 (8+i×80,96,72×30) 等 |
| 智能家居 | 空调、灯，点卡片切换 | 卡片 1 (8,40,304×74)、卡片 2 (8,122,304×74) |

触摸屏未装之前，可用串口单字符模拟点击（`main.cpp` 的 `debugKeys()`）：

```
w 主页→天气详情    a 主页→闹钟列表    e 列表→编辑第一条
c 主页→智能家居    b 返回上级
```

XPT2046 装上后，把换算后的屏幕坐标传给 `ui::tap(x, y)` 即可，页面逻辑不用改。

## 1. 项目简介

传统闹钟只能在本机按键设定，功能单一；市面上的智能音箱又依赖云端、断网即失效。
本项目以 ESP32-S3 为核心，做一台"**云端配置、本地执行**"的闹钟：

- 手机 H5 页面远程增删闹钟、设定重复规则；
- 闹钟时间表同时存在云端与设备 NVS，**断网照常响铃**；
- 顶部一颗按键完成"取消 / 懒睡 / 起夜灯"三种操作；
- 板载红外发射与 433 MHz 继电器，把闹钟变成房间的本地控制器与网关；
- 2.4 寸彩屏显示时间与天气，喇叭支持自定义铃声与渐强音量。

## 2. 功能特性

| 模块 | 功能 | 状态 |
|---|---|---|
| 远程闹钟 | 云端增 / 删 / 改，支持按周重复（含工作日）、法定节假日跳过 | ✅ 完成 |
| 本地闹钟 | NVS 持久化，断网可响；上电恢复；云端改动自动同步 | ✅ 完成 |
| 显示 | 时间、日期、天气、空气指数、下个闹钟、Wi-Fi 名称、世界时钟、昼夜地图 | ✅ 完成 |
| 网页端 | 自适应 H5（电脑两栏 / 手机单栏）、登录鉴权、闹钟 iOS 风格编辑、设置即改即下发 | ✅ 完成 |
| 灯光 | 继电器控制 12 V LED 灯开 / 关，状态回传 | ✅ 完成 |
| 云端 | 腾讯云轻量服务器：REST + MQTT + 登录鉴权 + 语音意图 | ✅ 完成 |
| 天气代理 | 云端合并天气+空气质量并按坐标缓存 30 分钟，设备一次请求取回；云端不可用时自动回退直连 | ✅ 完成 |
| 按键 | 单击取消 / 双击起夜灯 / 长按懒睡（引脚已定义，状态机未写） | ⏳ 进行中 |
| 音频 | WAV 铃声，30 秒内音量 20%→100% 渐强 | ⏳ 未做 |
| 红外 | 学习并回放空调遥控码（Coolix / 格力） | ⏳ 未做 |
| 语音 | 按键录音 → 腾讯云 ASR → 意图 → MQTT 下发（仅云端与模拟器打通） | ⏳ 进行中 |

## 3. 系统架构

```
手机 H5 ──HTTPS/REST──> Nginx ──> FastAPI ──> MySQL
                                    │
                                    └──MQTT(TLS 8883)──> EMQX
                                                          │
                                              ┌───────────┴───────────┐
                                              │      ESP32-S3         │
                                              │  net / core / ui / bsp │
                                              └───────────┬───────────┘
                                                          │
                        ┌──────────┬──────────┬───────────┼──────────┬──────────┐
                      TFT 屏     按键      喇叭       红外收发     继电器     12V 灯
```

设计要点：

1. **双数据源**：云端是配置源，设备是执行源；MQTT 下行带版本号，设备按版本收敛。
2. **离线优先**：闹钟判定完全在设备侧完成，网络只负责同步，不参与实时触发。
3. **单一事实出处**：引脚在 `include/pins.h`，协议在 `docs/protocol.md`，接线在 `docs/hardware.md`。

## 4. 硬件清单

| 名称 | 型号 / 规格 | 数量 | 参考价 | 作用 |
|---|---|---|---|---|
| 主控 | ESP32-S3 DevKitC-1 N16R8（16 MB Flash / 8 MB PSRAM） | 1 | ¥55 | 主控 + Wi-Fi |
| 显示屏 | 2.4" ILI9341 240×320 SPI | 1 | ¥35 | 时间 / 天气 |
| 音频功放 | MAX98357A I2S | 1 | ¥12 | 铃声 |
| 喇叭 | 4 Ω 3 W | 1 | ¥8 | 发声 |
| 继电器 | 1 路 5 V，高电平触发，10 A 触点 | 1 | ¥10 | 灯光开关 |
| LED 灯 | 12 V 3 W E27 球泡 + 灯座 | 1 | ¥15 | 被控灯 |
| 红外接收 | VS1838B | 1 | ¥1.5 | 学习遥控码 |
| 红外发射 | 940 nm LED + S8050 | 1 | ¥3.5 | 控制空调 |
| 按键 | 6×6 轻触 | 1 | ¥1 | 顶部按钮 |
| 电源 | 5 V/3 A + 12 V/1 A | 2 | ¥37 | 逻辑 / 灯 |
| 洞洞板、端子、杜邦线 | — | 1 批 | ¥45 | 装配 |
| 外壳 | 3D 打印或亚克力 | 1 | ¥50 | 结构 |

### 引脚分配

| 功能 | GPIO | 备注 |
|---|---|---|
| TFT CS / DC / RST | 10 / 8 / 9 | SPI |
| TFT MOSI / SCK / MISO | 11 / 12 / 13 | 40 MHz |
| TFT 背光 | 7 | 经 S8050，或直连 3.3 V |
| 顶部按键 | 1 | 内部上拉，按下接地 |
| 红外接收 / 发射 | 2 / 21 | 发射经 S8050 |
| 继电器 | 14 | 高电平吸合 |
| 蜂鸣器（可选） | 15 | — |

> 禁用：GPIO 26–37（Flash/PSRAM）、0/3/45/46（strapping）、19/20（USB）、43/44（UART）。

## 5. 目录结构

```
alarm-clock/
├── README.md                     # 本文件
├── .gitignore
├── .github/workflows/build.yml   # push 时自动 pio run 验证
│
├── docs/                         # 文档与论文素材
│   ├── scoping_en.docx           # 开题报告（英）✅
│   ├── scoping_cn.docx           # 开题报告（中）✅
│   ├── protocol.md               # MQTT 主题与 JSON 字段（唯一出处）
│   ├── hardware.md               # 接线表、供电预算、实物照片
│   ├── figures/                  # 架构图、状态机、甘特图
│   └── notes/                    # 每周记录，写论文时直接摘
│
├── firmware/                     # PlatformIO 工程 ✅
│   ├── platformio.ini
│   ├── boards/                   # 自定义 N16R8 板定义（待补）
│   ├── include/
│   │   ├── pins.h                # 引脚唯一出处
│   │   ├── config.h              # 常量与默认值
│   │   ├── secrets.h             # Wi-Fi / MQTT 凭据（git 忽略）
│   │   └── app_state.h           # 跨任务共享状态
│   ├── src/
│   │   ├── main.cpp              # 初始化顺序 + 创建任务
│   │   ├── bsp/                  # 驱动层：tft / button / audio / ir / relay
│   │   ├── core/                 # 业务层：alarm_engine / settings / clock
│   │   ├── net/                  # 通信层：wifi / mqtt / ota
│   │   └── ui/                   # 界面层：pages / widgets
│   ├── data/                     # LittleFS：铃声 wav、图标、字体
│   ├── lib/                      # 自写可复用库
│   └── test/                     # 纯逻辑单测（闹钟引擎）
│
├── server/                       # 腾讯云后端（规划）
│   ├── docker-compose.yml        # Nginx + FastAPI + MySQL + EMQX
│   ├── requirements.txt
│   ├── .env.example
│   ├── app/
│   │   ├── main.py
│   │   ├── api/                  # alarms / devices / auth
│   │   ├── models/               # SQLAlchemy
│   │   ├── mqtt/                 # REST ↔ MQTT 桥接
│   │   └── services/             # weather / scheduler
│   └── tests/
│
├── web/                          # 手机 H5（规划）
│   ├── index.html
│   ├── src/                      # main.js / api.js / ui.js
│   └── styles.css
│
└── hardware/                     # 实物资料（规划）
    ├── bom.xlsx
    ├── wiring.png
    └── datasheets/
```

**分层依赖只允许单向**：`main → ui / net / core → bsp`。`bsp/` 不得引用上层任何头文件。

## 6. 快速开始

### 环境要求

- VS Code + PlatformIO IDE（或 PlatformIO Core ≥ 6.2）
- Python 3.10+（PlatformIO 自带）
- 国内网络需代理，本机实测 **HTTP 代理 127.0.0.1:7897**

```bash
export HTTPS_PROXY=http://127.0.0.1:7897
export HTTP_PROXY=http://127.0.0.1:7897
```

### 编译与烧录

```bash
cd firmware
pio run                    # 编译
pio run -t upload          # 烧录（连不上时按住 BOOT）
pio device monitor         # 串口，115200
```

首次编译会自动下载工具链（约 300 MB）与 Arduino 框架（约 250 MB），耗时 5–10 分钟。

### 配置

```bash
cp include/secrets.example.h include/secrets.h   # 填入 Wi-Fi / MQTT
```

`platformio.ini` 关键项：

```ini
platform = espressif32@6.13.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.partitions = default_16MB.csv
board_build.arduino.memory_type = qio_opi
```

## 7. 通信协议（摘要）

主题前缀 `alarm/<device_id>/`，QoS 1，TLS 8883，遗嘱主题 `state`。

| 主题 | 方向 | 说明 |
|---|---|---|
| `cmd` | 云 → 设备 | 增删改闹钟、灯控、空调、对时 |
| `state` | 设备 → 云（保留） | 当前闹钟表、灯状态、在线状态 |
| `event` | 设备 → 云 | 响铃、按键、错误 |

```json
{
  "v": 12,
  "type": "alarm.set",
  "id": "a1",
  "time": "07:30",
  "repeat_mask": 31,
  "enabled": true,
  "ringtone": "sunrise.wav"
}
```

`repeat_mask` 为 7 位，bit0 = 周一。

## 8. 开发路线

| 阶段 | 内容 | 状态 |
|---|---|---|
| P0 | 工具链、工程骨架、Git | ✅ 完成 |
| P1 | ILI9341 点亮与自检 | 🔄 进行中 |
| P2 | 按键消抖与手势状态机 | 待开始 |
| P3 | MAX98357A 音频与渐强 | 待开始 |
| P4 | 红外学习与空调码回放 | 待开始 |
| P5 | 继电器与 12 V 灯 | 待开始 |
| P6 | SNTP、闹钟引擎、NVS | 待开始 |
| P7 | 云端 REST + MQTT + H5 | 待开始 |
| P8 | 联调、断网测试、外壳 | 待开始 |

## 远程仓库（两个）

| 名称 | 地址 | 用途 |
|---|---|---|
| `origin` | `git@github.com:LiuWTQAQ/alarm-clock.git` | GitHub，备份与展示 |
| `gitee` | `git@gitee.com:LiuWT-Yoshino/alarm-clock.git` | 腾讯云服务器从这个地址拉取 |

两边必须同步，否则会出现“Gitee 有、GitHub 没有”。日常提交：

```bash
git add -A && git commit -m "..." && git sync   # 一次推到两个远端
```

`git sync` 是仓库内配置的别名（依次 push origin、push gitee）；每日自动提交脚本也会同时推两个远端。

## 9. 开发规范

- **提交**：一个逻辑改动一次 commit，信息用 `feat/fix/docs/chore(scope): 描述`；里程碑打 tag（`v0.2-display`）。
- **分支**：`main` 保持可编译；功能分支 `feat/xxx`，合并前自测。
- **代码**：文件名 snake_case；头文件用 `#pragma once`；常量进 `config.h`；禁止魔法数字。
- **依赖**：新增 `lib_deps` 后执行 `PlatformIO: Rebuild IntelliSense Index`。
- **安全**：`secrets.h` 与 `.env` 永不入库。

## 10. 常见问题

**Q1 屏幕白屏 / 花屏？**
先查 CS/DC/RST 三根是否接反，再把 `SPI_FREQUENCY` 从 40 MHz 降到 20 MHz。

**Q2 VS Code 里 `#include <Arduino.h>` 标红，但 `pio run` 成功？**
IntelliSense 未同步：命令面板执行 `PlatformIO: Rebuild IntelliSense Index`，仍红则 `Developer: Reload Window`。

**Q3 `pio run` 卡在下载工具链？**
检查代理端口（本机 7897），并在同一终端 export 后再执行；`pio run -v` 可看到具体 URL。

**Q4 板子被识别成 N8、无 PSRAM？**
PlatformIO 内置板定义默认 N8；需自定义 `boards/` 定义或用 `board_upload.flash_size`。当前不影响功能。

## 11. 文档索引

| 文档 | 说明 |
|---|---|
| `docs/scoping_cn.docx` | 开题报告（中文） |
| `docs/scoping_en.docx` | 开题报告（英文） |
| `docs/protocol.md` | MQTT 与 REST 契约 |
| `docs/hardware.md` | 接线表与供电预算 |

---

© 2026 刘文韬 · 西南交通大学—利兹学院 · 指导教师：黄慧萍

## 网页配置（不用重新烧写）

设备连上 Wi-Fi 后自带一个配置页，电脑/手机与闹钟在同一局域网即可打开：

```
http://192.168.31.173/          # 串口日志里会打印实际 IP
http://alarm.local/             # mDNS 名字（部分路由器不转发 .local，用 IP 更稳）
```

页面上能做两件事：

| 操作 | 存储位置 | 生效方式 |
|---|---|---|
| 选择天气城市（40 个城市） | NVS `cfg/city` | 保存后立刻按新经纬度拉取天气 |
| 上传背景图（JPEG） | LittleFS `/bg.jpg` | 上传完成立即重绘，重启后仍在 |

约束：背景图只接受 JPEG（ESP32 上没有 PNG 解码器），建议 320×240 或其整数倍；
绘制时会按比例自动降采样并压暗 50%，保证时间/文字可读。

相关源码：`src/core/settings.cpp`（城市表 + NVS）、`src/net/webcfg.cpp`（HTTP 服务与上传）、
`src/ui/ui.cpp` 里的 `drawBgImg()`（TJpg_Decoder 解码 + 压暗 + 上屏）。

## 分区表

`partitions.csv` 把 8 MB Flash 划成：app 3 MB + LittleFS 960 KB（`spiffs` 标签）。
改动过分区表后，第一次烧录前建议先 `pio run -t erase`，否则旧的文件系统数据可能对不上。

## 字体

屏上中文来自 `include/ui_font_cn.h`（点阵字库，由 `tools/gen_cnfont.py` 从 macOS 自带的
华文黑体 Medium 生成，三档 28/18/15 px）。之所以不用微软雅黑：15 px 下雅黑笔画太细，
二值化后会断笔；华文黑体 Medium 的笔画更均匀，在 1-bit 点阵下更清楚。

## 界面与交互（多页面）

| 页面 | 入口 | 内容 |
|------|------|------|
| 主页 | 开机 | 日期 / 时钟（24 或 12 小时制）/ 进度条 / 天气卡 / 闹钟卡 / 家居 + 设置两个入口 |
| 天气详情 | 点天气卡 | 温度、天气、体感温度、湿度、未来三天最高/最低 |
| 闹钟列表 | 点闹钟卡 | 最多 5 条，时间 + 重复 + 开关；点单条进编辑，底部“添加闹钟” |
| 编辑闹钟 | 列表点单条 | 时/分加减、重复模式循环切换、开关、删除、保存 |
| 家居 | 点“家居” | 空调（红外）、灯（继电器）两张大卡片，点击切换 |
| 设置 | 点“设置” | 24/12 小时制、系统语言（中文/English）、天气最近更新时间 |

交互目前用触摸坐标 `ui::tap(x, y)` 实现；触摸屏未接时可用串口单字符模拟点击：

| 键 | 作用 |
|----|------|
| `w` / `a` / `c` | 主页 → 天气详情 / 闹钟列表 / 家居 |
| `e` | 闹钟列表 → 编辑第一条 |
| `s` | 主页 → 设置 |
| `t` / `l` | 设置页切换 12/24 小时制 / 语言 |
| `b` | 返回主页 |
| `q` | 把屏幕 320×100 区域以 hex 回读到串口（调试用） |

接 XPT2046 触摸后，在触摸回调里调用 `ui::tap(x, y)` 即可，无需改页面代码。
