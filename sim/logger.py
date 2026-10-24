"""CSV per-run logging for replay — writes under gitignored logs/."""

from __future__ import annotations

import csv
from pathlib import Path


class CsvRunLogger:
    HEADER = ("time_ms", "state", "energy_mwh", "altitude_cm")

    def __init__(self, path: str | Path) -> None:
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._rows: list[tuple[int, str, int, int]] = []

    def log(self, time_ms: int, state: str, energy_mwh: int, altitude_cm: int) -> None:
        self._rows.append((time_ms, state, energy_mwh, altitude_cm))

    def flush(self) -> Path:
        with self.path.open("w", encoding="utf-8", newline="") as fh:
            writer = csv.writer(fh)
            writer.writerow(self.HEADER)
            writer.writerows(self._rows)
        return self.path

    @property
    def row_count(self) -> int:
        return len(self._rows)
