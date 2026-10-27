#!/usr/bin/env python3
"""腾讯云「一句话识别」最小客户端：验证密钥 + 打通 ASR 链路。

用法:
    python3 tools/asr_test.py test.wav
密钥来源（按优先级）:
    环境变量 TENCENT_SECRET_ID / TENCENT_SECRET_KEY
    或 server/.env 里的同名键
"""
import base64, hashlib, hmac, json, os, sys, time

import requests
from datetime import datetime, timezone

HOST = "asr.tencentcloudapi.com"
SERVICE = "asr"
VERSION = "2019-06-14"
ACTION = "SentenceRecognition"


def load_env():
    sid = os.environ.get("TENCENT_SECRET_ID")
    key = os.environ.get("TENCENT_SECRET_KEY")
    if sid and key:
        return sid, key
    env = os.path.join(os.path.dirname(__file__), "..", "server", ".env")
    for line in open(env, encoding="utf-8"):
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            k, v = line.split("=", 1)
            if k == "TENCENT_SECRET_ID":
                sid = v.strip()
            elif k == "TENCENT_SECRET_KEY":
                key = v.strip()
    return sid, key


def sign(sid, key, action, payload):
    ts = int(time.time())
    date = datetime.fromtimestamp(ts, timezone.utc).strftime("%Y-%m-%d")
    body = json.dumps(payload, separators=(",", ":"), ensure_ascii=False)
    ct = "application/json; charset=utf-8"

    canonical_headers = f"content-type:{ct}\nhost:{HOST}\nx-tc-action:{action.lower()}\n"
    signed_headers = "content-type;host;x-tc-action"
    hashed = hashlib.sha256(body.encode()).hexdigest()
    canonical = f"POST\n/\n\n{canonical_headers}\n{signed_headers}\n{hashed}"
    scope = f"{date}/{SERVICE}/tc3_request"
    to_sign = f"TC3-HMAC-SHA256\n{ts}\n{scope}\n" + hashlib.sha256(canonical.encode()).hexdigest()

    def h(k, m):
        return hmac.new(k, m.encode(), hashlib.sha256).digest()

    k = h(("TC3" + key).encode(), date)
    k = h(k, SERVICE)
    k = h(k, "tc3_request")
    sig = hmac.new(k, to_sign.encode(), hashlib.sha256).hexdigest()
    auth = (f"TC3-HMAC-SHA256 Credential={sid}/{scope}, "
            f"SignedHeaders={signed_headers}, Signature={sig}")
    headers = {
        "Authorization": auth, "Content-Type": ct, "Host": HOST,
        "X-TC-Action": action, "X-TC-Version": VERSION,
        "X-TC-Timestamp": str(ts), "X-TC-Region": "ap-guangzhou",
    }
    return body, headers


def recognize(path):
    data = open(path, "rb").read()
    sid, key = load_env()
    payload = {
        "ProjectId": 0, "SubServiceType": 2, "EngSerViceType": "16k_zh",
        "SourceType": 1, "VoiceFormat": "wav", "DataLen": len(data),
        "Data": base64.b64encode(data).decode(),
    }
    body, headers = sign(sid, key, ACTION, payload)
    # trust_env=False：忽略 Clash 写入的系统代理（对国内域名会握手失败）
    sess = requests.Session(); sess.trust_env = False
    t0 = time.time()
    last = None
    for attempt in range(2):
        try:
            r = sess.post(f"https://{HOST}", data=body.encode(), headers=headers, timeout=30)
            res = r.json()
            break
        except Exception as e:            # 网络抖动/超时重试一次
            last = e
            time.sleep(0.8)
    else:
        print("NETWORK", type(last).__name__, str(last)[:80])
        return None
    el = time.time() - t0
    resp = res.get("Response", {})
    if "Error" in resp:
        print("ERROR", resp["Error"])
        return None
    print(f"[asr] {el:.2f}s  text={resp.get('Result')!r}  "
          f"audio={resp.get('AudioDuration')}ms  request={resp.get('RequestId')}")
    return resp.get("Result")


if __name__ == "__main__":
    p = sys.argv[1] if len(sys.argv) > 1 else "/tmp/t16k.wav"
    recognize(p)
