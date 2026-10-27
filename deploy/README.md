# 部署到腾讯云轻量服务器（与现有 WordPress 共存）

## 三条原则

1. **不改现有配置**：Nginx 只新增 `conf.d/alarm.conf` 一个文件，WordPress 的 server 块原样不动。
2. **不共用端口**：FastAPI 只监听 `127.0.0.1:8001`，公网 80/443 由 Nginx 按域名分流。
3. **不共用数据库**：用独立 SQLite（`/opt/alarm/data/app.db`），不碰现有 MySQL。

## 一次性准备

```bash
# 1. 独立系统用户与目录
sudo useradd -r -s /sbin/nologin alarm || true
sudo mkdir -p /opt/alarm/{releases,data}
sudo chown -R alarm:alarm /opt/alarm

# 2. 服务与 Nginx 配置
sudo cp deploy/alarm.service /etc/systemd/system/alarm.service
sudo cp deploy/nginx-alarm.conf /etc/nginx/conf.d/alarm.conf
sudo nginx -t && sudo systemctl reload nginx
sudo systemctl daemon-reload && sudo systemctl enable --now alarm

# 3. MQTT broker（2 GB 内存建议 Mosquitto，约 10 MB）
sudo dnf install -y mosquitto
sudo systemctl enable --now mosquitto
```

## 每次更新

```bash
cd ~/alarm-clock && git pull
sudo ./deploy/deploy.sh
```

脚本会：拷到 `releases/<时间戳>` → 切软链 → 重启服务 → 健康检查；失败自动回滚到上一个版本。

## 防火墙 / 端口

| 端口 | 用途 | 是否对公网 |
|---|---|---|
| 80 / 443 | Nginx（WordPress + alarm 子域） | 是（已开） |
| 8001 | FastAPI | 否，仅 127.0.0.1 |
| 1883 / 8883 | MQTT（设备接入） | 8883 需要放行 |
