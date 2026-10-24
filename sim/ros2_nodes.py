"""ROS 2 jazzy stubs: BatteryState + Image in, supervisor status out, mock camera."""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class BatteryState:
    percentage: int  # 0..100
    voltage_mv: int


@dataclass
class ImageFrame:
    width: int
    height: int
    stamp_ms: int
    ok: bool = True


@dataclass
class SupervisorStatus:
    state: str
    energy_mwh: int
    altitude_cm: int
    filming: bool
    message: str = ""


@dataclass
class MockRos2Bridge:
    """Stand-in for rclpy nodes until Isaac/Pegasus wiring."""

    battery: BatteryState = field(default_factory=lambda: BatteryState(100, 22000))
    last_image: ImageFrame | None = None
    status: SupervisorStatus | None = None
    camera_triggers: int = 0
    starlink_up: bool = True

    def publish_battery(self, percentage: int, voltage_mv: int) -> str:
        self.battery = BatteryState(percentage, voltage_mv)
        return f"ROS2_BATTERY pct={percentage} mv={voltage_mv}"

    def publish_image(self, width: int, height: int, stamp_ms: int, ok: bool = True) -> str:
        self.last_image = ImageFrame(width, height, stamp_ms, ok)
        return f"ROS2_IMAGE w={width} h={height} ok={1 if ok else 0}"

    def publish_status(self, status: SupervisorStatus) -> str:
        self.status = status
        return (
            f"ROS2_STATUS state={status.state} E={status.energy_mwh} "
            f"alt={status.altitude_cm} film={1 if status.filming else 0}"
        )

    def trigger_camera(self) -> str:
        if self.last_image is not None and not self.last_image.ok:
            return "CAMERA_TRIGGER_FAIL"
        self.camera_triggers += 1
        return f"CAMERA_TRIGGER n={self.camera_triggers}"

    def set_starlink(self, up: bool) -> str:
        self.starlink_up = up
        return f"STARLINK={'UP' if up else 'DOWN'}"
