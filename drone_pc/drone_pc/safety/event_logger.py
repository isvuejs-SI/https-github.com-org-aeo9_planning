from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import List

from drone_pc.params import Params


@dataclass
class LogEvent:
    timestamp_s: float
    category: str
    payload: str


class EventLogger:
    def __init__(self, params: Params):
        self.params = params
        self.events: List[LogEvent] = []

    def log_verdict(self, timestamp_s: float, payload: str) -> None:
        if not self.params.logging_log_verdicts:
            return
        self.events.append(LogEvent(timestamp_s, "verdict", payload))

    def log_transition(self, timestamp_s: float, payload: str) -> None:
        if not self.params.logging_log_transitions:
            return
        self.events.append(LogEvent(timestamp_s, "transition", payload))

    def directory(self) -> str:
        return self.params.logging_log_directory

    def flush_to_file(self, filename: str) -> bool:
        path = Path(filename)
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a", encoding="utf-8") as fh:
            for e in self.events:
                fh.write(f"{e.timestamp_s}\t{e.category}\t{e.payload}\n")
        return True
