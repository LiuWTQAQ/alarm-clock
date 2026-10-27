"""Alarm clock cloud API.

Run locally:   python3 server/app.py            (http://127.0.0.1:8000)
In production: uvicorn server.app:app --host 0.0.0.0 --port 8000

Transport: when MQTT_URL is set the service talks real MQTT (EMQX on the
Tencent Cloud host); without it the service runs an in-process bus so the
web UI and tools/sim_device.py can be exercised on a laptop.
"""
import hashlib, hmac, json, os, secrets, sqlite3, threading, time, uuid
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
import asr as asr_mod, intent as intent_mod

ROOT = Path(__file__).resolve().parent.parent
DB = Path(os.environ.get("ALARM_DB", ROOT / "server" / "data.db"))
MQTT_URL = os.environ.get("MQTT_URL", "")
DEV = "a943f8"                      # the only device for now

AUTH_USER = os.environ.get("ALARM_USER", "LiuWT")
AUTH_PASS = os.environ.get("ALARM_PASS", "L0219")
SESSION_TTL = 7 * 24 * 3600
COOKIE = "sid"

_PROD = os.environ.get("ALARM_PROD") == "1"
app = FastAPI(title="Alarm Clock API",
              docs_url=None if _PROD else "/docs",
              redoc_url=None if _PROD else "/redoc",
              openapi_url=None if _PROD else "/openapi.json")
_lock = threading.Lock()
_conn = sqlite3.connect(DB, check_same_thread=False)
_conn.row_factory = sqlite3.Row
_queue: dict[str, list] = {}        # dev -> list of {"topic","payload"}
_online: dict[str, bool] = {}


def db(sql, args=()):
    with _lock:
        cur = _conn.execute(sql, args)
        _conn.commit()
        return cur


def init():
    db("""CREATE TABLE IF NOT EXISTS devices(
            id TEXT PRIMARY KEY, name TEXT, online INT DEFAULT 0,
            fw TEXT, rev INT DEFAULT 0, state TEXT, last_seen INT DEFAULT 0)""")
    db("""CREATE TABLE IF NOT EXISTS alarms(
            dev TEXT, id INT, h INT, m INT, mask INT, on_ INT, label TEXT,
            PRIMARY KEY(dev, id))""")
    db("""CREATE TABLE IF NOT EXISTS events(
            id INTEGER PRIMARY KEY AUTOINCREMENT, dev TEXT, ts INT, type TEXT, payload TEXT)""")
    db("""CREATE TABLE IF NOT EXISTS commands(
            id TEXT PRIMARY KEY, dev TEXT, op TEXT, payload TEXT, status TEXT, ts INT)""")
    acols = [r["name"] for r in _conn.execute("PRAGMA table_info(alarms)").fetchall()]
    for name, decl in (("snooze", "INTEGER DEFAULT 1"),
                       ("snooze_min", "INTEGER DEFAULT 9"),
                       ("ring", "TEXT DEFAULT '故事时间'")):
        if name not in acols:
            db(f"ALTER TABLE alarms ADD COLUMN {name} {decl}")
    cols = [r["name"] for r in _conn.execute("PRAGMA table_info(devices)").fetchall()]
    if "settings" not in cols:
        db("ALTER TABLE devices ADD COLUMN settings TEXT DEFAULT '{}'")
    db("""CREATE TABLE IF NOT EXISTS users(
            user TEXT PRIMARY KEY, salt TEXT, pw TEXT)""")
    db("""CREATE TABLE IF NOT EXISTS sessions(
            token TEXT PRIMARY KEY, user TEXT, ts INT)""")
    if not _conn.execute("SELECT 1 FROM users WHERE user=?", (AUTH_USER,)).fetchone():
        salt = secrets.token_hex(16)
        db("INSERT INTO users(user,salt,pw) VALUES(?,?,?)",
           (AUTH_USER, salt, hash_pw(AUTH_PASS, salt)))

    if not _conn.execute("SELECT 1 FROM devices WHERE id=?", (DEV,)).fetchone():
        db("INSERT INTO devices(id,name,online,rev,state,last_seen) VALUES(?,?,0,0,?,0)",
           (DEV, "卧室闹钟", "{}"))
        for i, (h, m, mask, on, label) in enumerate(
                [(7, 30, 0x1F, 1, "起床"), (8, 10, 0x60, 0, "周末")], start=1):
            db("INSERT INTO alarms VALUES(?,?,?,?,?,?,?)", (DEV, i, h, m, mask, on, label))


# ---------------------------------------------------------------- auth
def hash_pw(pw: str, salt: str) -> str:
    return hashlib.pbkdf2_hmac("sha256", pw.encode(), bytes.fromhex(salt), 120_000).hex()


def check_login(user: str, pw: str) -> bool:
    row = _conn.execute("SELECT salt,pw FROM users WHERE user=?", (user,)).fetchone()
    if not row:
        hmac.compare_digest(hash_pw(pw, "00" * 16), hash_pw(pw, "00" * 16))
        return False
    return hmac.compare_digest(hash_pw(pw, row["salt"]), row["pw"])


def new_session(user: str) -> str:
    tok = secrets.token_urlsafe(32)
    db("INSERT INTO sessions(token,user,ts) VALUES(?,?,?)", (tok, user, int(time.time())))
    db("DELETE FROM sessions WHERE ts < ?", (int(time.time()) - SESSION_TTL,))
    return tok


def current_user(request: Request) -> Optional[str]:
    tok = request.cookies.get(COOKIE)
    if not tok:
        return None
    row = _conn.execute("SELECT user,ts FROM sessions WHERE token=?", (tok,)).fetchone()
    if not row or row["ts"] < int(time.time()) - SESSION_TTL:
        return None
    return row["user"]


PUBLIC = {"/api/login", "/api/health"}


@app.middleware("http")
async def auth_guard(request: Request, call_next):
    path = request.url.path
    if path.startswith("/api/") and path not in PUBLIC:
        if not current_user(request):
            return JSONResponse({"detail": "unauthorized"}, status_code=401)
    return await call_next(request)


# ---------------------------------------------------------------- transport
def bus_publish(topic: str, payload: dict):
    """Cloud -> device."""
    if MQTT_URL:
        _mqtt_publish(topic, payload)
    else:
        dev = topic.split("/")[1]
        _queue.setdefault(dev, []).append({"topic": topic, "payload": payload})


def device_message(dev: str, kind: str, payload: dict):
    """Device -> cloud, called by MQTT subscriber or by /api/sim/pub."""
    now = int(time.time())
    if kind == "state":
        db("UPDATE devices SET state=?, online=1, last_seen=?, rev=? WHERE id=?",
           (json.dumps(payload, ensure_ascii=False), now,
            payload.get("rev", 0), dev))
        _online[dev] = True
    elif kind == "event":
        db("INSERT INTO events(dev,ts,type,payload) VALUES(?,?,?,?)",
           (dev, payload.get("ts", now), payload.get("type", "?"),
            json.dumps(payload, ensure_ascii=False)))
        db("UPDATE devices SET last_seen=? WHERE id=?", (now, dev))
    elif kind == "ack":
        db("UPDATE commands SET status=? WHERE id=?",
           ("ok" if payload.get("ok") else "err:" + str(payload.get("err")), payload.get("id")))
    elif kind == "online":
        on = bool(payload.get("online"))
        _online[dev] = on
        db("UPDATE devices SET online=?, last_seen=? WHERE id=?", (1 if on else 0, now, dev))


def _mqtt_publish(topic, payload):
    import paho.mqtt.publish as publish
    publish.single(topic, json.dumps(payload, ensure_ascii=False), qos=1,
                   host=_mqtt_host(), port=_mqtt_port())


def _mqtt_host():
    return MQTT_URL.split("://")[-1].split(":")[0]


def _mqtt_port():
    parts = MQTT_URL.split("://")[-1].split(":")
    return int(parts[1]) if len(parts) > 1 else 1883


def mqtt_worker():
    import paho.mqtt.client as mqtt
    client = mqtt.Client(client_id="alarm-cloud")

    def on_message(_c, _u, msg):
        try:
            payload = json.loads(msg.payload.decode())
        except Exception:
            return
        parts = msg.topic.split("/")
        if len(parts) == 3:
            device_message(parts[1], parts[2], payload)

    client.on_message = on_message
    client.connect(_mqtt_host(), _mqtt_port(), 30)
    client.subscribe([("alarm/+/state", 1), ("alarm/+/event", 1),
                      ("alarm/+/ack", 1), ("alarm/+/online", 1)])
    client.loop_forever()


DEFAULT_SETTINGS = {
    "name": "卧室闹钟", "city": "成都", "tz": "Asia/Shanghai",
    "hour12": True, "fahrenheit": False, "wx_interval": 30, "lang": "zh",
}


# ---------------------------------------------------------------- models
class AlarmIn(BaseModel):
    h: int
    m: int
    mask: int = 0
    on: bool = True
    label: str = ""
    snooze: bool = True
    snooze_min: int = 9
    ring: str = "故事时间"


# ---------------------------------------------------------------- auth routes
class LoginIn(BaseModel):
    username: str
    password: str


_fails: dict[str, list] = {}


@app.post("/api/login")
def login(body: LoginIn, request: Request):
    ip = request.client.host if request.client else "?"
    now = time.time()
    hits = [t for t in _fails.get(ip, []) if now - t < 300]
    if len(hits) >= 8:
        raise HTTPException(429, "尝试次数过多，请 5 分钟后再试")
    if not check_login(body.username, body.password):
        hits.append(now)
        _fails[ip] = hits
        raise HTTPException(401, "用户名或密码不正确")
    _fails.pop(ip, None)
    tok = new_session(body.username)
    resp = JSONResponse({"ok": True, "user": body.username})
    resp.set_cookie(COOKIE, tok, max_age=SESSION_TTL, httponly=True, samesite="lax",
                    secure=request.headers.get("x-forwarded-proto") == "https")
    return resp


@app.post("/api/logout")
def logout(request: Request):
    tok = request.cookies.get(COOKIE)
    if tok:
        db("DELETE FROM sessions WHERE token=?", (tok,))
    resp = JSONResponse({"ok": True})
    resp.delete_cookie(COOKIE)
    return resp


@app.get("/api/me")
def me(request: Request):
    return {"user": current_user(request)}


@app.get("/api/health")
def health():
    return {"ok": True, "ts": int(time.time())}


# ---------------------------------------------------------------- REST
@app.get("/api/devices")
def devices():
    rows = db("SELECT id,name,online,fw,rev,last_seen FROM devices").fetchall()
    return [dict(r) for r in rows]


@app.get("/api/devices/{dev}/state")
def state(dev: str):
    row = db("SELECT state,rev,online,last_seen FROM devices WHERE id=?", (dev,)).fetchone()
    if not row:
        raise HTTPException(404, "device not found")
    st = json.loads(row["state"] or "{}")
    st["rev"] = row["rev"]
    st["online"] = bool(row["online"]) and (time.time() - row["last_seen"] < 30)
    return st


@app.get("/api/devices/{dev}/settings")
def get_settings(dev: str):
    row = db("SELECT settings FROM devices WHERE id=?", (dev,)).fetchone()
    if not row:
        raise HTTPException(404, "device not found")
    st = dict(DEFAULT_SETTINGS)
    st.update(json.loads(row["settings"] or "{}"))
    return st


@app.put("/api/devices/{dev}/settings")
async def put_settings(dev: str, body: dict):
    row = db("SELECT settings FROM devices WHERE id=?", (dev,)).fetchone()
    if not row:
        raise HTTPException(404, "device not found")
    cur = dict(DEFAULT_SETTINGS)
    cur.update(json.loads(row["settings"] or "{}"))
    changed = {k: body[k] for k in body if k in DEFAULT_SETTINGS and body[k] != cur.get(k)}
    cur.update(changed)
    db("UPDATE devices SET settings=? WHERE id=?",
       (json.dumps(cur, ensure_ascii=False), dev))
    if "name" in changed:
        db("UPDATE devices SET name=? WHERE id=?", (str(changed["name"])[:12], dev))
    if changed:
        cmd = {"id": str(uuid.uuid4()), "op": "config.set", "ts": int(time.time()), **changed}
        db("INSERT INTO commands VALUES(?,?,?,?,?,?)",
           (cmd["id"], dev, "config.set", json.dumps(cmd, ensure_ascii=False), "sent", cmd["ts"]))
        bus_publish(f"alarm/{dev}/cmd", cmd)
    return {"settings": cur, "pushed": changed}


def alarm_row(r):
    return {"id": r["id"], "h": r["h"], "m": r["m"], "mask": r["mask"],
            "on": bool(r["on_"]), "label": r["label"],
            "snooze": bool(r["snooze"]), "snooze_min": r["snooze_min"],
            "ring": r["ring"]}


@app.get("/api/devices/{dev}/alarms")
def alarms(dev: str):
    rows = db("SELECT * FROM alarms WHERE dev=? ORDER BY id", (dev,)).fetchall()
    return [alarm_row(r) for r in rows]


def _sync(dev: str):
    """Push the whole alarm table to the device and bump the revision."""
    rev = db("SELECT rev FROM devices WHERE id=?", (dev,)).fetchone()["rev"] + 1
    db("UPDATE devices SET rev=? WHERE id=?", (rev, dev))
    rows = db("SELECT * FROM alarms WHERE dev=? ORDER BY id", (dev,)).fetchall()
    items = [alarm_row(r) for r in rows]
    cmd = {"id": str(uuid.uuid4()), "op": "alarm.sync", "ts": int(time.time()),
           "rev": rev, "alarms": items}
    db("INSERT INTO commands VALUES(?,?,?,?,?,?)",
       (cmd["id"], dev, "alarm.sync", json.dumps(cmd, ensure_ascii=False), "sent", cmd["ts"]))
    bus_publish(f"alarm/{dev}/cmd", cmd)
    return rev


@app.post("/api/devices/{dev}/alarms")
def alarm_add(dev: str, a: AlarmIn):
    nxt = db("SELECT COALESCE(MAX(id),0)+1 n FROM alarms WHERE dev=?", (dev,)).fetchone()["n"]
    db("INSERT INTO alarms(dev,id,h,m,mask,on_,label,snooze,snooze_min,ring) "
       "VALUES(?,?,?,?,?,?,?,?,?,?)",
       (dev, nxt, a.h % 24, a.m % 60, a.mask, 1 if a.on else 0, a.label[:9],
        1 if a.snooze else 0, max(1, min(30, a.snooze_min)), a.ring[:12]))
    return {"id": nxt, "rev": _sync(dev)}


@app.put("/api/devices/{dev}/alarms/{aid}")
def alarm_update(dev: str, aid: int, a: AlarmIn):
    db("UPDATE alarms SET h=?,m=?,mask=?,on_=?,label=?,snooze=?,snooze_min=?,ring=? "
       "WHERE dev=? AND id=?",
       (a.h % 24, a.m % 60, a.mask, 1 if a.on else 0, a.label[:9],
        1 if a.snooze else 0, max(1, min(30, a.snooze_min)), a.ring[:12], dev, aid))
    return {"rev": _sync(dev)}


@app.delete("/api/devices/{dev}/alarms/{aid}")
def alarm_del(dev: str, aid: int):
    db("DELETE FROM alarms WHERE dev=? AND id=?", (dev, aid))
    return {"rev": _sync(dev)}


@app.post("/api/devices/{dev}/cmd")
async def cmd(dev: str, body: dict):
    body = dict(body)
    body.setdefault("id", str(uuid.uuid4()))
    body.setdefault("ts", int(time.time()))
    db("INSERT INTO commands VALUES(?,?,?,?,?,?)",
       (body["id"], dev, body.get("op", "?"), json.dumps(body, ensure_ascii=False),
        "sent", body["ts"]))
    bus_publish(f"alarm/{dev}/cmd", body)
    return {"id": body["id"], "status": "sent"}


@app.post("/api/voice")
async def voice(request: Request, dev: str = DEV, execute: bool = True):
    """上传一段 wav（body 原始字节），识别 -> 解析意图 -> 可选下发命令。

    返回 {text, intent, params, reply, command, timing}
    """
    wav = await request.body()
    if len(wav) < 1000:
        raise HTTPException(400, "audio too short")
    if len(wav) > 1_500_000:
        raise HTTPException(413, "audio too large")
    try:
        r = asr_mod.recognize(wav)
    except asr_mod.AsrError as e:
        raise HTTPException(502, f"asr failed: {e}")
    parsed = intent_mod.parse(r["text"])
    out = {"text": r["text"], "intent": None, "params": {}, "reply": None,
           "command": None, "asr_ms": r["cost_ms"], "audio_ms": r["duration_ms"]}
    if not parsed:
        out["reply"] = "没听懂，请再说一遍"
        return out
    intent, params, reply = parsed
    if intent == "query.device" and not reply:
        row = db("SELECT state FROM devices WHERE id=?", (dev,)).fetchone()
        st = json.loads(row["state"]) if row and row["state"] else {}
        if params.get("target") == "light":
            reply = "灯是开着的" if (st.get("light") or {}).get("on") else "灯是关着的"
        else:
            ac = st.get("ac") or {}
            reply = (f"空调开着，{ac.get('temp', 26)} 度" if ac.get("on") else "空调是关着的")
    if intent == "query.time" and not reply:
        reply = time.strftime("现在是 %H:%M", time.localtime())
    elif intent == "query.weather" and not reply:
        row = db("SELECT state FROM devices WHERE id=?", (dev,)).fetchone()
        st = json.loads(row["state"]) if row and row["state"] else {}
        env = st.get("env") or {}
        if env.get("temp") is not None:
            reply = f"现在 {env.get('temp')} 度，{env.get('cond', '')}".strip()
        else:
            reply = "还没拿到天气数据"
    out.update(intent=intent, params=params, reply=reply)
    cmd = intent_mod.to_command(intent, params)
    if cmd and execute:
        cmd.update({"id": str(uuid.uuid4()), "ts": int(time.time())})
        db("INSERT INTO commands VALUES(?,?,?,?,?,?)",
           (cmd["id"], dev, cmd["op"], json.dumps(cmd, ensure_ascii=False), "sent", cmd["ts"]))
        bus_publish(f"alarm/{dev}/cmd", cmd)
        out["command"] = cmd
    return out


@app.get("/api/devices/{dev}/events")
def events(dev: str, limit: int = 30):
    rows = db("SELECT ts,type,payload FROM events WHERE dev=? ORDER BY id DESC LIMIT ?",
              (dev, limit)).fetchall()
    return [dict(r) for r in rows]


# ---------------------------------------------------------------- simulator hooks
@app.post("/api/sim/pub")
async def sim_pub(body: dict):
    """Device simulator -> cloud (stands in for the MQTT uplink)."""
    device_message(body.get("dev", DEV), body.get("kind", "state"), body.get("payload", {}))
    return {"ok": True}


@app.get("/api/sim/poll")
def sim_poll(dev: str = DEV):
    return {"messages": _queue.pop(dev, [])}


@app.post("/api/sim/online")
def sim_online(body: dict):
    device_message(body.get("dev", DEV), "online", {"online": body.get("online", True)})
    return {"ok": True}


# ---------------------------------------------------------------- static
app.mount("/", StaticFiles(directory=str(ROOT / "web"), html=True), name="web")

init()
if MQTT_URL:
    threading.Thread(target=mqtt_worker, daemon=True).start()

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="127.0.0.1", port=int(os.environ.get("PORT", 8000)))
