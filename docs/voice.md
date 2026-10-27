# 语音控制（分支 feat/voice）

## 一句话链路

```
麦克风 INMP441 ──I2S1──► ESP32-S3 ──HTTPS──► alarm.yoshino-liuwt.top /api/voice
                                                    │
                                      腾讯云一句话识别（ASR）
                                                    │
                                          规则意图解析 intent.py
                                                    │
                                    MQTT light.set / ac.set ──► 设备执行
                                                    └─► 返回 text/intent/reply，设备播报回执
```

## 已跑通（本机，2026-10-27）

| 语音 | 识别文本 | 意图 | 下发命令 | 端到端 |
|---|---|---|---|---|
| 打开灯 | 打开灯。 | light.on | `{"op":"light.set","on":true}` | 200 OK |
| 小助手，帮我把灯打开 | … | light.on | 同上 | 200 OK |
| 现在几点了 | 现在几点了？ | query.time | — | 回复「现在是 11:58」 |
| 空调调到26度 | — | ac.temp | `{"op":"ac.set","on":true,"temp":26}` | 待接真机 |

ASR 实测耗时（服务器侧 `asr_ms`）：**450–720 ms**（2.7 s 音频），偶尔网络抖动会到 3 s 以上，已加一次重试。

## 接口

### `POST /api/voice`

- 鉴权：同网页，需登录 Cookie
- Body：wav 原始字节（16 kHz / 16 bit / 单声道），Content-Type: `audio/wav`
- Query：`dev`（默认 a943f8）、`execute=0` 只识别不下发

```json
{
  "text": "打开灯。",
  "intent": "light.on",
  "params": {},
  "reply": "好的，已开灯",
  "command": {"op": "light.set", "on": true, "id": "…", "ts": 1793073564},
  "asr_ms": 453,
  "audio_ms": 944
}
```

### 意图表（`server/intent.py`）

| 说法 | intent | 命令 |
|---|---|---|
| 开灯 / 把灯打开 / 亮一点 | light.on | `light.set on=true` |
| 关灯 / 把灯关掉 / 暗一点 | light.off | `light.set on=false` |
| 开空调 / 启动空调 | ac.on | `ac.set on=true` |
| 关空调 | ac.off | `ac.set on=false` |
| 空调调到 N 度（支持「二十六度」） | ac.temp | `ac.set on=true temp=N` |
| 几点了 / 现在什么时候 | query.time | 回「现在是 HH:MM」 |
| 天气怎么样 | query.weather | 回设备上报的温湿度 |

识别不了统一回「没听懂，请再说一遍」。

## 密钥

放在 `server/.env`（已 gitignore）：

```
TENCENT_SECRET_ID=…
TENCENT_SECRET_KEY=…
```

- 服务器上同样放一份，权限 `chmod 600`
- **不要提交、不要截图**；答辩演示前检查 `git status`
- 建议在腾讯云控制台给这个密钥改成「仅 ASR 读写」的子账号策略

## 还没做

1. **硬件**：INMP441 接 I2S1（BCLK 18 / WS 38 / SD 39），独立按键 GPIO40 触发
2. **固件**：I2S 录音 → WAV → HTTPS POST；收到 reply 后用 MAX98357A 播 TTS
3. **TTS**：目前只有文字回执，可用腾讯云语音合成返回 wav 直接推 I2S（不用解码 mp3）
4. **真机 MQTT**：`light.set` / `ac.set` 需要固件先有 MQTT 客户端，当前只在模拟器生效
5. **唤醒词**（可选）：ESP-SR WakeNet，需转 ESP-IDF 或找 Arduino 移植

## 测量与调优记录

- 一句话识别限 60 s、≤ 3 MB；我们用 3 s、约 96 KB，远低于限制
- 首次请求 TLS 握手约 0.6–1.8 s，脚本已用 keep-alive 会话复用
- macOS 系统代理（Clash）会让 Python/设备请求国内域名握手失败：代码里统一 `trust_env=False` / `ProxyHandler({})`
