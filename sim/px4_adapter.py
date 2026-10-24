"""PX4 adapter: one interface over MAVSDK; mock for tests. Sim-only."""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum


class FlightCommand(str, Enum):
    ARM = "ARM"
    TAKEOFF = "TAKEOFF"
    GOTO = "GOTO"
    HOLD = "HOLD"
    RETURN = "RETURN"
    LAND = "LAND"


@dataclass
class MockVehicleState:
    armed: bool = False
    mode: str = "IDLE"
    north_cm: int = 0
    east_cm: int = 0
    alt_cm: int = 0
    history: list[str] = field(default_factory=list)


class Px4Adapter(ABC):
    """Wrap MAVSDK (arm, takeoff, goto, hold, return, land)."""

    @abstractmethod
    def arm(self) -> str: ...

    @abstractmethod
    def takeoff(self, alt_cm: int) -> str: ...

    @abstractmethod
    def goto(self, north_cm: int, east_cm: int, alt_cm: int) -> str: ...

    @abstractmethod
    def hold(self) -> str: ...

    @abstractmethod
    def return_to_launch(self) -> str: ...

    @abstractmethod
    def land(self) -> str: ...


class MockPx4Adapter(Px4Adapter):
    """In-process mock used by unit/scenario tests (no real MAVLink)."""

    def __init__(self) -> None:
        self.state = MockVehicleState()

    def arm(self) -> str:
        self.state.armed = True
        self.state.mode = "ARMED"
        msg = "PX4_ARM"
        self.state.history.append(msg)
        return msg

    def takeoff(self, alt_cm: int) -> str:
        self.state.alt_cm = alt_cm
        self.state.mode = "TAKEOFF"
        msg = f"PX4_TAKEOFF alt_cm={alt_cm}"
        self.state.history.append(msg)
        return msg

    def goto(self, north_cm: int, east_cm: int, alt_cm: int) -> str:
        self.state.north_cm = north_cm
        self.state.east_cm = east_cm
        self.state.alt_cm = alt_cm
        self.state.mode = "GOTO"
        msg = f"PX4_GOTO n={north_cm} e={east_cm} alt={alt_cm}"
        self.state.history.append(msg)
        return msg

    def hold(self) -> str:
        self.state.mode = "HOLD"
        msg = "PX4_HOLD"
        self.state.history.append(msg)
        return msg

    def return_to_launch(self) -> str:
        self.state.north_cm = 0
        self.state.east_cm = 0
        self.state.mode = "RTL"
        msg = "PX4_RTL"
        self.state.history.append(msg)
        return msg

    def land(self) -> str:
        self.state.alt_cm = 0
        self.state.armed = False
        self.state.mode = "LAND"
        msg = "PX4_LAND"
        self.state.history.append(msg)
        return msg


class MavsdkPx4Adapter(Px4Adapter):
    """Placeholder for real MAVSDK wiring — disabled until hardware (November)."""

    def __init__(self) -> None:
        raise RuntimeError("MavsdkPx4Adapter disabled: sim_only until hardware")

    def arm(self) -> str:  # pragma: no cover
        raise RuntimeError("sim_only")

    def takeoff(self, alt_cm: int) -> str:  # pragma: no cover
        raise RuntimeError("sim_only")

    def goto(self, north_cm: int, east_cm: int, alt_cm: int) -> str:  # pragma: no cover
        raise RuntimeError("sim_only")

    def hold(self) -> str:  # pragma: no cover
        raise RuntimeError("sim_only")

    def return_to_launch(self) -> str:  # pragma: no cover
        raise RuntimeError("sim_only")

    def land(self) -> str:  # pragma: no cover
        raise RuntimeError("sim_only")
