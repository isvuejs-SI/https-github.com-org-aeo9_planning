"""In-process stand-in for the ROS 2 supervisor node. No rclpy.

Subscribes to BatteryState and Image stand-ins, publishes supervisor status,
and mocks the camera trigger. Image bytes are not stored.
"""


class RosMock(object):
    def __init__(self):
        self.soc = 0
        self.mv = 0
        self.frame = 0
        self.close = 0
        self.conf = 0
        self.status = "NOMINAL"
        self.reason = "ok"
        self.triggers = 0
        self.fails = 0

    def on_battery(self, soc, mv):
        self.soc = soc
        self.mv = mv
        return "battery %d %d" % (soc, mv)

    def on_image(self, frame, close, conf):
        self.frame = frame
        self.close = close
        self.conf = conf
        return "image %d %d %d" % (frame, close, conf)

    def publish(self, status, reason):
        self.status = status
        self.reason = reason
        return "status %s %s" % (status, reason)

    def trigger(self, ack):
        self.triggers += 1
        if ack:
            return "ACK"
        self.fails += 1
        return "NACK"
