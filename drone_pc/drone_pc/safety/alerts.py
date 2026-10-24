from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List

from drone_pc.params import Params


@dataclass
class AlertRecord:
    key: str
    message: str
    timestamp_s: float = 0.0
    sent: bool = False
    reason: str = ""


class AlertManager:
    def __init__(self, params: Params):
        self.params = params
        self._last_sent_s: Dict[str, float] = {}
        self.history: List[AlertRecord] = []

    def ntfy_url(self) -> str:
        return f"{self.params.alerts_ntfy_base_url}/{self.params.alerts_ntfy_topic}"

    def maybe_send(self, key: str, message: str, now_s: float) -> AlertRecord:
        rec = AlertRecord(key=key, message=message, timestamp_s=now_s)
        if not self.params.alerts_enabled:
            rec.reason = "alerts_disabled"
            self.history.append(rec)
            return rec

        last = self._last_sent_s.get(key)
        if last is not None and (now_s - last) < self.params.alerts_duplicate_holdback_s:
            rec.reason = "duplicate_holdback"
            self.history.append(rec)
            return rec

        self._last_sent_s[key] = now_s
        rec.sent = True
        rec.reason = "sent_stub"
        self.history.append(rec)
        return rec
