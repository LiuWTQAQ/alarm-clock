"""腾讯云「一句话识别」客户端（TC3-HMAC-SHA256 签名，仅用标准库 + requests）。

密钥从环境变量或 server/.env 读取，绝不入库。
"""
import base64, hashlib, hmac, json, os, time
from datetime import datetime, timezone
from pathlib import Path

import requests

HOST = "asr.tencentcloudapi.com"
SERVICE = "asr"
VERSION = "2019-06-14"
ACTION = "SentenceRecognition"
REGION = os.environ.get("ASR_REGION", "ap-guangzhou")
ENV_FILE = Path(__file__).resolve().parent / ".env"


def _load_env():
    sid, key = os.environ.get("TENCENT_SECRET_ID"), os.environ.get("TENCENT_SECRET_KEY")
    if sid and key:
        return sid, key
    if ENV_FILE.exists():
        for line in ENV_FILE.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                if k == "TENCENT_SECRET_ID":
                    sid = v.strip()
                elif k == "TENCENT_SECRET_KEY":
                    key = v.strip()
    return sid, key


def _sign(sid: str, key: str, payload: dict) -> tuple[bytes, dict]:
    ts = int(time.time())
    date = datetime.fromtimestamp(ts, timezone.utc).strftime("%Y-%m-%d")
    body = json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode()
    ct = "application/json; charset=utf-8"
    canonical_headers = f"content-type:{ct}\nhost:{HOST}\nx-tc-action:{ACTION.lower()}\n"
    signed_headers = "content-type;host;x-tc-action"
    canonical = (f"POST\n/\n\n{canonical_headers}\n{signed_headers}\n"
                 + hashlib.sha256(body).hexdigest())
    scope = f"{date}/{SERVICE}/tc3_request"
    to_sign = (f"TC3-HMAC-SHA256\n{ts}\n{scope}\n"
               + hashlib.sha256(canonical.encode()).hexdigest())

    def h(k: bytes, m: str) -> bytes:
        return hmac.new(k, m.encode(), hashlib.sha256).digest()

    k = h(("TC3" + key).encode(), date)
    k = h(k, SERVICE)
    k = h(k, "tc3_request")
    sig = hmac.new(k, to_sign.encode(), hashlib.sha256).hexdigest()
    headers = {
        "Authorization": (f"TC3-HMAC-SHA256 Credential={sid}/{scope}, "
                          f"SignedHeaders={signed_headers}, Signature={sig}"),
        "Content-Type": ct, "Host": HOST,
        "X-TC-Action": ACTION, "X-TC-Version": VERSION,
        "X-TC-Timestamp": str(ts), "X-TC-Region": REGION,
    }
    return body, headers


class AsrError(RuntimeError):
    pass


def recognize(wav: bytes, fmt: str = "wav", engine: str = "16k_zh") -> dict:
    """返回 {text, duration_ms, request_id, cost_ms}；失败抛 AsrError。"""
    sid, key = _load_env()
    if not sid or not key:
        raise AsrError("missing_credentials")
    payload = {
        "ProjectId": 0, "SubServiceType": 2, "EngSerViceType": engine,
        "SourceType": 1, "VoiceFormat": fmt, "DataLen": len(wav),
        "Data": base64.b64encode(wav).decode(),
    }
    body, headers = _sign(sid, key, payload)
    sess = requests.Session()
    sess.trust_env = False           # 系统代理对国内域名会握手失败
    t0 = time.time()
    last = None
    for _ in range(2):
        try:
            r = sess.post(f"https://{HOST}", data=body, headers=headers, timeout=30)
            res = r.json().get("Response", {})
            break
        except Exception as e:       # 网络抖动重试一次
            last = e
            time.sleep(0.6)
    else:
        raise AsrError(f"network: {type(last).__name__}")
    if "Error" in res:
        raise AsrError(f"{res['Error'].get('Code')}: {res['Error'].get('Message')}")
    return {
        "text": (res.get("Result") or "").strip(),
        "duration_ms": res.get("AudioDuration", 0),
        "request_id": res.get("RequestId"),
        "cost_ms": int((time.time() - t0) * 1000),
    }
