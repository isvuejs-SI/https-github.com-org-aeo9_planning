"""PX4 request interface. Mock only: no socket, no MAVSDK, no live MAVLink.

The supervisor may request arm, takeoff, goto, hold, return, or land.
PX4 keeps its own failsafes; nothing here clears them.
"""


class Px4Mock(object):
    def __init__(self):
        self.mode = "DISARMED"
        self.armed = 0
        self.x = 0
        self.y = 0
        self.z = 0
        self.requests = 0

    def connect(self, url):
        return "REFUSE simulation_only"

    def arm(self):
        self.requests += 1
        self.armed = 1
        self.mode = "ARMED"
        return "ACK"

    def takeoff(self, alt):
        self.requests += 1
        if not self.armed:
            return "REFUSE not_armed"
        self.mode = "TAKEOFF"
        self.z = alt
        return "ACK"

    def goto(self, x, y, z):
        self.requests += 1
        if not self.armed:
            return "REFUSE not_armed"
        self.mode = "GOTO"
        self.x = x
        self.y = y
        self.z = z
        return "ACK"

    def hold(self):
        self.requests += 1
        if not self.armed:
            return "REFUSE not_armed"
        self.mode = "HOLD"
        return "ACK"

    def do_return(self):
        self.requests += 1
        if not self.armed:
            return "REFUSE not_armed"
        self.mode = "RETURN"
        return "ACK"

    def land(self):
        self.requests += 1
        self.mode = "LAND"
        self.z = 0
        self.armed = 0
        return "ACK"
