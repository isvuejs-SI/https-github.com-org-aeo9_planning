"""Requests only. The Pixhawk PX4 stack owns the final failsafe."""

from dataclasses import dataclass


@dataclass
class Px4Command:
    wire: str = "none"
    command: int = 0
    param1: float = None
    param2: float = None
    param3: float = None
    latch_no_arm: bool = False


def request_rank(wire):
    return {"none": 0, "hold": 1, "rtl": 2, "land": 3, "no_arm": 4}.get(wire, 0)


def fill_command(wire, latch):
    cmd = Px4Command(wire=wire, latch_no_arm=latch)
    if wire == "hold":
        cmd.command = 176
        cmd.param1, cmd.param2, cmd.param3 = 1, 4, 3
    elif wire == "rtl":
        cmd.command = 20
    elif wire == "land":
        cmd.command = 21
    elif wire == "no_arm":
        cmd.command = 400
        cmd.param1 = 0
    return cmd


def _latch(verdict):
    return verdict in {"NO_TAKEOFF", "NO_GO", "PROP_LOCKOUT", "HOLD_GROUND", "DETER"}


def map_request(verdict, airborne):
    # CLIMB_ESCAPE and ESCAPE are logged verdicts. This package never sends an arm.
    if verdict in {"CONTINUE", "GO", "GO_SIM", "STAY", "WATCH", "ESCAPE", "CLIMB_ESCAPE"}:
        return fill_command("none", False)
    latch = _latch(verdict)
    if airborne:
        if verdict in {"HOLD", "HOLD_GROUND", "DETER"}:
            wire = "hold"
        elif verdict in {"RTL", "ABORT_MISSION"}:
            wire = "rtl"
        elif verdict in {"LAND", "LAND_NOW", "NO_TAKEOFF", "NO_GO", "PROP_LOCKOUT"}:
            wire = "land"
        else:
            wire = "none"
        return fill_command(wire, latch)
    if verdict == "HOLD":
        return fill_command("hold", False)
    if verdict == "RTL":
        return fill_command("rtl", False)
    if verdict in {"LAND", "LAND_NOW"}:
        return fill_command("land", False)
    if verdict == "ABORT_MISSION":
        return fill_command("no_arm", True)
    if latch:
        return fill_command("no_arm", True)
    return fill_command("none", False)


def combine_requests(parts, airborne):
    best = "none"
    latch = False
    for part in parts:
        latch = latch or part.latch_no_arm
        if request_rank(part.wire) > request_rank(best):
            best = part.wire
    if airborne and best == "no_arm":
        best = "land"
        latch = True
    return fill_command(best, latch)


def mavsdk_action(wire):
    return {
        "hold": "action.hold",
        "rtl": "action.return_to_launch",
        "land": "action.land",
        "no_arm": "action.disarm",
    }.get(wire, "none")


def command_signature(cmd):
    if cmd.wire == "none":
        return cmd.wire
    text = f"{cmd.wire} cmd={cmd.command}"
    if cmd.param1 is not None:
        text += f" p1={int(cmd.param1)}"
    if cmd.param2 is not None:
        text += f" p2={int(cmd.param2)}"
    if cmd.param3 is not None:
        text += f" p3={int(cmd.param3)}"
    return text


def dispatch_mavsdk(wire, transport):
    """Call MAVSDK only when that transport is selected and a connection string is set.

    The uXRCE-DDS request is published by the node either way. A missing library
    does not invent a flight command.
    """
    import os

    connection = os.environ.get("MAVSDK_CONNECTION", "")
    action = mavsdk_action(wire)
    if transport != "mavsdk" and not connection:
        return "uxrce"
    try:
        import mavsdk  # noqa: F401
    except ImportError:
        return "mavsdk-missing"
    if not connection:
        return "mavsdk-no-connection:" + action
    if wire == "none":
        return "mavsdk-none"
    try:
        import asyncio
        from mavsdk import System

        async def go():
            drone = System()
            await drone.connect(system_address=connection)
            async for state in drone.core.connection_state():
                if state.is_connected:
                    break
            if wire == "hold":
                await drone.action.hold()
            elif wire == "rtl":
                await drone.action.return_to_launch()
            elif wire == "land":
                await drone.action.land()
            elif wire == "no_arm":
                await drone.action.disarm()

        asyncio.run(asyncio.wait_for(go(), timeout=3))
        return "mavsdk-sent:" + action
    except Exception as exc:
        print(f"mavsdk request failed: {exc}")
        return "mavsdk-failed"


def command_json(cmd, uxrce_topic):
    import json
    body = {
        "wire": cmd.wire,
        "command": cmd.command,
        "latch_no_arm": cmd.latch_no_arm,
        "uxrce_topic": uxrce_topic,
        "note": "request only; PX4 owns the failsafe",
        "mavsdk": mavsdk_action(cmd.wire),
    }
    if cmd.param1 is not None:
        body["param1"] = cmd.param1
    if cmd.param2 is not None:
        body["param2"] = cmd.param2
    if cmd.param3 is not None:
        body["param3"] = cmd.param3
    return json.dumps(body, separators=(",", ":"))
