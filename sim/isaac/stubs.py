"""PX4 SITL + Pegasus Simulator placeholders (software-only / sim-first)."""

from __future__ import annotations


class PegasusSimStub:
    """Stand-in for Pegasus airframe spawn in Isaac Sim."""

    def __init__(self) -> None:
        self.running = False

    def start_sitl(self) -> str:
        self.running = True
        return "PEGASUS_SITL_STUB_STARTED"

    def stop(self) -> str:
        self.running = False
        return "PEGASUS_SITL_STUB_STOPPED"


class IsaacWorldStub:
    def __init__(self) -> None:
        self.loaded = False

    def load_world(self, usd_path: str) -> str:
        self.loaded = True
        return f"ISAAC_WORLD_STUB path={usd_path}"
