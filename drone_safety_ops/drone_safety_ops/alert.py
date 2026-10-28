import os
import re
import time
import urllib.request

PRIO = {"WATCH": "default", "DETER": "high", "ESCAPE": "urgent", "PROP_LOCKOUT": "urgent"}
_TOPIC = re.compile(r"^[A-Za-z0-9_.-]{1,128}$")


def topic_from_env(default_topic):
    return os.environ.get("NTFY_TOPIC") or default_topic


def topic_ok(topic):
    return bool(topic) and _TOPIC.match(topic) is not None


class Alerter:
    """60 s duplicate hold-back. STAY never alerts and does not clear the last verdict."""

    def __init__(self, holdback_s, clock=None, sender=None):
        self.holdback_s = holdback_s
        self.clock = clock or time.time
        self.sender = sender
        self.last = None
        self.last_t = 0

    def alert(self, verdict, msg, dry=False):
        now = self.clock()
        if verdict == "STAY" or (verdict == self.last and now - self.last_t < self.holdback_s):
            return False, ""
        if verdict not in PRIO:
            return False, ""
        self.last = verdict
        self.last_t = now
        line = f"ALERT {PRIO[verdict]} {verdict} {msg}"
        if dry:
            return True, line
        if self.sender and self.sender(verdict, msg, PRIO[verdict]):
            return True, line
        return False, line


def send_ntfy(host, topic, verdict, msg, timeout=5):
    if not topic_ok(topic) or not topic_ok(host):
        print("alert failed, logged locally: bad ntfy topic")
        return False
    url = f"https://{host}/{topic}"
    req = urllib.request.Request(
        url,
        data=msg.encode(),
        headers={"Title": f"Drone {verdict}", "Priority": PRIO[verdict]},
    )
    try:
        urllib.request.urlopen(req, timeout=timeout)
        return True
    except Exception as exc:  # network failures stay local
        print(f"alert failed, logged locally: {exc}")
        return False
