# 部署到宝塔（腾讯云轻量 · OpenCloudOS 9.4 · nginx 1.28）

现有环境：宝塔面板 + nginx 1.28 + MariaDB + PHP（WordPress 在 80/443），Python 3.11.6。
本项目的原则：**不碰 WordPress 的站点配置、不共用端口、不共用数据库**。

## 一、DNS

腾讯云 DNSPod 添加记录：

| 主机记录 | 类型 | 值 |
|---|---|---|
| `alarm` | A | `119.91.35.90` |

子域名沿用主域名备案，无需重新备案。

## 二、宝塔建站（只做一次）

1. **网站 → 添加站点**：域名 `alarm.yoshino-liuwt.top`，PHP 版本选“纯静态”，不创建数据库。
2. **站点设置 → SSL**：Let's Encrypt 申请证书，开启“强制 HTTPS”。
3. **站点设置 → 反向代理 → 添加反向代理**：
   - 代理名称：`alarm-api`
   - 目标 URL：`http://127.0.0.1:8001`
   - 发送域名：`$host`
4. 宝塔会自动在 `/www/server/panel/vhost/nginx/alarm.yoshino-liuwt.top.conf` 里生成 `location /` 反代，**不要手改 WordPress 的 conf**。

## 三、后端服务（SSH）

```bash
# 独立用户与目录
useradd -r -s /sbin/nologin alarm 2>/dev/null || true
mkdir -p /opt/alarm/{releases,data}
chown -R alarm:alarm /opt/alarm

# 虚拟环境（系统自带 Python 3.11）
python3 -m venv /opt/alarm/venv
/opt/alarm/venv/bin/pip install -q --upgrade pip
/opt/alarm/venv/bin/pip install -q fastapi 'uvicorn[standard]' paho-mqtt

# systemd
cp deploy/alarm.service /etc/systemd/system/alarm.service
systemctl daemon-reload
systemctl enable --now alarm
systemctl status alarm --no-pager
```

## 四、首次发布

```bash
cd ~/alarm-clock        # 或 /root/alarm-clock
./deploy/deploy.sh
curl -s http://127.0.0.1:8001/api/devices
```

## 五、验证

```bash
curl -I https://alarm.yoshino-liuwt.top            # 200 + 证书正常
curl -s https://alarm.yoshino-liuwt.top/api/devices
```

浏览器打开 `https://alarm.yoshino-liuwt.top` 应看到 H5 控制页。

## 六、以后每次更新

```bash
cd ~/alarm-clock && git pull && ./deploy/deploy.sh
```

## 七、端口

| 端口 | 用途 | 公网 |
|---|---|---|
| 80 / 443 | 宝塔 nginx | 已开 |
| 8001 | FastAPI | 否（127.0.0.1） |
| 1883 / 8883 | MQTT（接设备时再开） | 8883 需在腾讯云 + 宝塔防火墙放行 |
