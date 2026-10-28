"""Local verdict log and ntfy payloads.

No tokens are stored or sent. HTTP delivery is off unless enable_send is
set in YAML, and even then the request carries no Authorization header.
"""

from __future__ import annotations

import os

from drone_pc.common.units import scale_milli


def build_ntfy(base_url: str, topic: str, title: str, message: str) -> dict:
    base = base_url[:-1] if base_url.endswith("/") else base_url
    return {
        "url": base + "/" + topic,
        "body": {"topic": topic, "title": title, "message": message},
    }


class AlertSink:
    def __init__(self, cooldown_s: float):
        self.cooldown_ms = scale_milli(cooldown_s)
        self.last: dict[str, int] = {}
        self.lines: list[str] = []

    def consider(self, now_ms: int, key: str, title: str, message: str, base_url: str, topic: str) -> dict:
        previous = self.last.get(key)
        if previous is not None and int(now_ms) - previous < self.cooldown_ms:
            line = f"t={int(now_ms)} kind=alert key={key} action=suppressed"
            self.lines.append(line)
            return {"sent": False, "line": line, "request": None}
        self.last[key] = int(now_ms)
        request = build_ntfy(base_url, topic, title, message)
        line = f"t={int(now_ms)} kind=alert key={key} action=send"
        self.lines.append(line)
        return {"sent": True, "line": line, "request": request}

    def log_verdict(self, now_ms: int, name: str, reason: str) -> str:
        line = f"t={int(now_ms)} kind=verdict name={name} reason={reason}"
        self.lines.append(line)
        return line


def write_events(directory: str, lines: list[str]) -> str:
    os.makedirs(directory, exist_ok=True)
    path = os.path.join(directory, "events.log")
    with open(path, "w", encoding="utf-8") as handle:
        for line in lines:
            handle.write(line + "\n")
    return path


def read_events(directory: str) -> list[str]:
    path = os.path.join(directory, "events.log")
    with open(path, "r", encoding="utf-8") as handle:
        return [line.rstrip("\n") for line in handle.readlines()]
