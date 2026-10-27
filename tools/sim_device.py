"""Stand-in for the ESP32: speaks the same protocol over HTTP while the
real device is away.  Keeps an alarm table, answers cloud commands, pushes
state every 2 s and can fire an alarm event.

    python3 tools/sim_device.py            # runs against localhost:8000
    python3 tools/sim_device.py --fire 1   # emit an alarm.fire event now
"""
import argparse, http.cookiejar, json, os, time, urllib.error, urllib.request

DEV = "a943f8"
USER = os.environ.get("ALARM_USER", "LiuWT")
PASS = os.environ.get("ALARM_PASS", "L0219")

_jar = http.cookiejar.CookieJar()
_opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(_jar))
_logged: dict = {"base": None}


def login(base):
    body = json.dumps({"username": USER, "password": PASS}).encode()
    req = urllib.request.Request(base + "/api/login", data=body, method="POST",
                                 headers={"Content-Type": "application/json"})
    with _opener.open(req, timeout=5) as r:
        r.read()
    _logged["base"] = base


def call(base, path, payload=None, method="GET", retry=True):
    if _logged["base"] != base:
        login(base)
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(base + path, data=data, method=method,
                                 headers={"Content-Type": "application/json"})
    try:
        with _opener.open(req, timeout=5) as r:
            return json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        if e.code == 401 and retry:
            login(base)
            return call(base, path, payload, method, retry=False)
        raise


class Device:
    def __init__(self, base, dev):
        self.base, self.dev = base, dev
        self.alarms = []
        self.light = {"on": False}
        self.ac = {"on": False, "temp": 26}
        self.rev = 0
        self.state = {}

    def push(self):
        now = time.localtime()
        payload = {
            "ts": int(time.time()), "rev": self.rev, "fw": "sim-0.1",
            "alarm": {"on": True, "ringing": False},
            "alarms": self.alarms,
            "light": self.light, "ac": self.ac,
            "env": {"t": 16, "h": 67, "aqi": 98, "cond": "阴"},
            "net": {"rssi": -62, "ip": "192.168.31.173"},
        }
        payload["clock"] = "%02d:%02d:%02d" % (now.tm_hour, now.tm_min, now.tm_sec)
        self.state = payload
        call(self.base, "/api/sim/pub", {"dev": self.dev, "kind": "state", "payload": payload}, "POST")

    def apply(self, cmd):
        op = cmd.get("op")
        ok, err = True, None
        if op == "alarm.sync":
            self.alarms = cmd.get("alarms", [])
            self.rev = cmd.get("rev", self.rev)
            print(f"  <- alarm.sync rev={self.rev} ({len(self.alarms)} alarms)")
        elif op == "light.set":
            self.light = {"on": bool(cmd.get("on"))}
            print(f"  <- light.set on={self.light['on']}")
        elif op == "ac.send":
            if cmd.get("key") == "power":
                self.ac["on"] = not self.ac["on"]
            elif cmd.get("key") == "temp":
                self.ac["temp"] = max(16, min(30, self.ac["temp"] + 1))
            print(f"  <- ac.send {cmd.get('key')} -> {self.ac}")
        elif op == "time.sync":
            pass
        else:
            ok, err = False, "unknown_op"
        ack = {"id": cmd.get("id"), "ok": ok, "ts": int(time.time())}
        if err:
            ack["err"] = err
        call(self.base, "/api/sim/pub", {"dev": self.dev, "kind": "ack", "payload": ack}, "POST")

    def fire(self):
        ev = {"type": "alarm.fire", "ts": int(time.time()), "alarm_id": 1}
        call(self.base, "/api/sim/pub", {"dev": self.dev, "kind": "event", "payload": ev}, "POST")
        print("  -> alarm.fire")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://127.0.0.1:8000")
    ap.add_argument("--dev", default=DEV)
    ap.add_argument("--fire", type=int, default=0, help="fire an alarm event after N seconds")
    a = ap.parse_args()

    d = Device(a.base, a.dev)
    call(a.base, "/api/sim/online", {"dev": a.dev, "online": True}, "POST")
    print(f"simulator for {a.dev} against {a.base}")
    start = time.time()
    while True:
        for m in call(a.base, f"/api/sim/poll?dev={a.dev}")["messages"]:
            if m["topic"].endswith("/cmd"):
                d.apply(m["payload"])
        if a.fire and time.time() - start > a.fire:
            d.fire(); a.fire = 0
        d.push()
        time.sleep(2)


if __name__ == "__main__":
    main()
