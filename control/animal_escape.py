"""Animal-escape: climb 35 m when detector confident; stop filming after 3."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class AnimalEscapeController:
    climb_cm: int
    stop_after: int
    escape_count: int = 0
    filming_allowed: bool = True
    last_climb_cm: int = 0

    def on_detection(self, confident: bool, close: bool) -> str:
        if not (confident and close):
            return "ANIMAL_NONE"
        self.escape_count += 1
        self.last_climb_cm = self.climb_cm
        if self.escape_count >= self.stop_after:
            self.filming_allowed = False
            return (
                f"ANIMAL_ESCAPE count={self.escape_count} "
                f"climb_cm={self.climb_cm} filming=0"
            )
        return (
            f"ANIMAL_ESCAPE count={self.escape_count} "
            f"climb_cm={self.climb_cm} filming=1"
        )

    def dump_line(self) -> str:
        return (
            f"animal escapes={self.escape_count} "
            f"filming={1 if self.filming_allowed else 0} "
            f"last_climb_cm={self.last_climb_cm}"
        )
