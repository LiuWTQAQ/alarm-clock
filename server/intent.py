"""把一句话解析成设备命令。

只做规则匹配，够用且可解释；识别不了就返回 None，让上层回一句“没听懂”。
"""
import re

CN_NUM = {"零": 0, "一": 1, "二": 2, "两": 2, "三": 3, "四": 4, "五": 5,
          "六": 6, "七": 7, "八": 8, "九": 9, "十": 10}


def _num(s: str):
    s = s.strip()
    if s.isdigit():
        return int(s)
    if s in CN_NUM:
        return CN_NUM[s]
    m = re.fullmatch(r"([一二两三四五六七八九])?十([一二三四五六七八九])?", s)
    if m:
        return (CN_NUM.get(m.group(1), 1) * 10) + CN_NUM.get(m.group(2), 0)
    if len(s) == 2 and s[0] in "一二两三四五六七八九" and s[1] in "一二三四五六七八九":
        return CN_NUM[s[0]] * 10 + CN_NUM[s[1]]
    return None


def parse(text: str):
    """-> (intent, params, reply) 或 None"""
    t = (text or "").strip().rstrip("。！？!?，,")
    if not t:
        return None

    if re.search(r"(关|闭|灭).{0,3}(灯|台灯|顶灯)", t) or re.search(r"(灯).{0,2}(关|灭)", t):
        return "light.off", {}, "好的，已关灯"
    if re.search(r"(开|亮).{0,3}(灯|台灯|顶灯)", t) or re.search(r"(灯).{0,2}(开|亮)", t):
        return "light.on", {}, "好的，已开灯"

    m = re.search(r"(空调|冷气).{0,6}?(\d{2}|[一二两三四五六七八九]?十[一二三四五六七八九]?|[零一二两三四五六七八九])度", t)
    if m:
        n = _num(m.group(2))
        if n and 16 <= n <= 30:
            return "ac.temp", {"value": n}, f"好的，空调调到 {n} 度"
    if re.search(r"(关|闭).{0,3}(空调|冷气)", t):
        return "ac.off", {}, "好的，已关空调"
    if re.search(r"(开|启动).{0,3}(空调|冷气)", t):
        return "ac.on", {}, "好的，已开空调"

    if re.search(r"(几点|时间|现在什么时候)", t):
        return "query.time", {}, None
    if re.search(r"(天气|气温|温度多少|外面冷|外面热)", t):
        return "query.weather", {}, None
    if re.search(r"(开灯|亮一点)", t):
        return "light.on", {}, "好的，已开灯"
    if re.search(r"(关灯|暗一点)", t):
        return "light.off", {}, "好的，已关灯"
    return None


def to_command(intent: str, params: dict):
    """intent -> 设备协议里的 cmd（与网页端一致）"""
    if intent == "light.on":
        return {"op": "light.set", "on": True}
    if intent == "light.off":
        return {"op": "light.set", "on": False}
    if intent == "ac.on":
        return {"op": "ac.set", "on": True}
    if intent == "ac.off":
        return {"op": "ac.set", "on": False}
    if intent == "ac.temp":
        return {"op": "ac.set", "on": True, "temp": params.get("value")}
    return None
