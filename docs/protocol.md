# 设备 ↔ 云端通信协议 v0.1

面向 ESP32 闹钟（下称“设备”）与腾讯云服务器（下称“云端”）之间的 MQTT + REST 协议。
目标：链路不可靠时闹钟仍然可靠响铃，恢复后状态自动对齐。

## 1. 总体结构

```
 手机 H5 ──HTTPS/REST──► 云端 API ──MQTT/TLS──► 设备
    ▲                        │
    └──── WebSocket/SSE ─────┘   （状态回推）
```

- **配置源在云端，执行源在设备**：云端维护闹钟列表，设备把闹钟写入 NVS；断网时设备按本地表响铃，联网后用版本号对齐。
- **状态源在设备**：设备上报 `state`（retain），云端据此渲染界面，不猜测设备状态。

## 2. MQTT 主题

统一前缀 `alarm/{devId}`，`devId` 为设备 MAC 后 6 位小写十六进制（如 `a943f8`）。

| 主题 | 方向 | QoS | retain | 用途 |
|---|---|---|---|---|
| `alarm/{devId}/cmd` | 云 → 设备 | 1 | 否 | 控制命令 |
| `alarm/{devId}/ack` | 设备 → 云 | 1 | 否 | 命令执行结果 |
| `alarm/{devId}/state` | 设备 → 云 | 1 | **是** | 全量状态（影子） |
| `alarm/{devId}/event` | 设备 → 云 | 1 | 否 | 事件上报（响铃、按键等） |
| `alarm/{devId}/online` | 设备 → 云 | 1 | **是** | 在线标记，LWT 写 `offline` |

连接参数：TLS 8883，`clientId = devId`（同时只允许一个连接），`keepalive = 30`，`cleanSession = false`。
遗嘱消息：`alarm/{devId}/online` ← `{"online":false,"ts":<epoch>}`，retain。

## 3. 消息格式

所有负载为 UTF-8 JSON，字段名小写下划线。

### 3.1 cmd（云端 → 设备）

```json
{"id": "6f1c…", "op": "alarm.sync", "ts": 1761523200, "rev": 12, "alarms": [ … ]}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | string | 命令唯一 id（UUID），用于幂等 |
| `op` | string | 见下表 |
| `ts` | int | 云端发送时间（epoch 秒） |
| 其余 | — | 随 `op` 变化 |

| op | 附加字段 | 说明 |
|---|---|---|
| `alarm.sync` | `rev`, `alarms[]` | 全量下发闹钟表，设备整体替换 |
| `alarm.cancel` | `alarm_id` | 立即停止当前响铃 |
| `alarm.snooze` | `minutes` | 贪睡 N 分钟 |
| `light.set` | `on`, `brightness`(0-100, 可选) | 灯开关/亮度 |
| `ac.send` | `key`, `code` | 红外码（`key` 为功能名，`code` 为 32 位十六进制） |
| `config.set` | `city`, `hour12`, `lang`, `fahrenheit` | 设备偏好 |
| `time.sync` | `epoch` | 强制校时（设备平时用 SNTP） |

### 3.2 ack（设备 → 云端）

```json
{"id": "6f1c…", "ok": true, "ts": 1761523202}
{"id": "6f1c…", "ok": false, "err": "nvs_full", "ts": 1761523202}
```

错误码：`bad_json`、`unknown_op`、`nvs_full`、`hw_fault`、`busy`、`stale_rev`。

### 3.3 state（设备 → 云端，retain）

```json
{
  "ts": 1761523200,
  "rev": 12,
  "fw": "0.4.0",
  "alarm": {"on": true, "ringing": false, "snooze_until": 0},
  "alarms": [{"id": 1, "h": 7, "m": 30, "mask": 31, "on": true, "label": "起床"}],
  "light": {"on": false, "brightness": 0},
  "ac": {"on": false, "mode": "cool", "temp": 26},
  "env": {"t": 16, "h": 67, "aqi": 98},
  "net": {"rssi": -62, "ip": "192.168.31.173", "uptime": 3600}
}
```

### 3.4 event（设备 → 云端）

```json
{"type": "alarm.fire", "ts": 1761523200, "alarm_id": 1}
{"type": "button",    "ts": 1761523205, "action": "cancel|snooze|lamp"}
{"type": "boot",      "ts": 1761520000, "reason": "power_on"}
```

## 4. 闹钟对象与重复规则

```json
{"id": 1, "h": 7, "m": 30, "mask": 31, "on": true, "label": "起床"}
```

`mask` 为 7 位掩码，bit0 = 周一 … bit6 = 周日：

| mask | 含义 |
|---|---|
| `0x00` | 一次 |
| `0x1F` | 工作日（周一至周五，跳法定节假日与周末，调休上班日照响） |
| `0x60` | 周末 |
| `0x7F` | 每天 |
| 其它 | 自定义 |

`label` 最长 9 个 UTF-8 字节（设备侧固定 `char[10]`）。

## 5. 一致性与离线

1. 云端每次修改闹钟 `rev += 1`，然后发 `alarm.sync`。
2. 设备写入 NVS 成功 → ack `ok` + 更新 `state.rev`；失败 → ack `err`，云端保留旧状态并在界面提示。
3. 设备重连后先发 `state`（含 `rev`）；云端比较：若设备 `rev < 云端 rev`，立刻补发 `alarm.sync`。
4. 断网期间设备按 NVS 表响铃，事件在恢复后补发（设备侧环形缓冲 16 条）。

## 6. REST 接口（H5 使用）

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/devices` | 设备列表与在线状态 |
| GET | `/api/devices/{id}/state` | 最新影子状态 |
| GET/POST | `/api/devices/{id}/alarms` | 查询 / 新增闹钟 |
| PUT/DELETE | `/api/devices/{id}/alarms/{aid}` | 修改 / 删除闹钟 |
| POST | `/api/devices/{id}/cmd` | 下发任意命令（灯、空调、取消响铃） |
| GET | `/api/devices/{id}/events?limit=50` | 最近事件 |
| GET | `/api/stream` | SSE，推送状态变化 |

鉴权：`Authorization: Bearer <token>`，首个版本用单用户固定 token，写在服务端 `.env`。

## 7. 待定

- OTA：`alarm/{devId}/ota` 主题 + 分片校验（v0.3 之后）。
- 多设备：`/api/devices` 已按列表设计，界面暂只展示一台。

## 配置下发

偏好设置（城市、时区、时间格式、温度单位、刷新间隔）由网页写入云端，再以 `config.set` 下发：

```json
{"id":"<uuid>","op":"config.set","ts":1793000000,
 "city":"上海","tz":"Asia/Shanghai","hour12":true,"fahrenheit":true,"lang":"zh"}
```

设备收到后写 NVS（`cfg` 命名空间），无需重启即可生效；只下发**变化**的字段。

`lang` 取值 `zh` / `en`，设备侧映射为 `cfg/lang` 的 `0` / `1`（0=中文，1=英文）。

### 闹钟字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `h` / `m` | int | 时分（0–23 / 0–59） |
| `mask` | int | 周一为 bit0，`31`=工作日、`127`=每天、`96`=周末、`0`=一次 |
| `on` | bool | 是否启用 |
| `label` | str | 标签，最多 9 字符 |
| `snooze` / `snooze_min` | bool / int | 稍后提醒开关与时长（分钟） |
| `ring` | str | 铃声名（`故事时间`/`清晨`/`鸟鸣`/`经典`/`渐强`） |

设备端 P2 阶段只需实现 `mask` 与 `snooze`；`ring` 在音频阶段映射到不同 WAV。
