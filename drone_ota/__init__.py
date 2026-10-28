"""OTA gate, signed app bundles, A/B slots, and boot health.

PX4 on the Pixhawk is not updated here. MAVLink is telemetry and requests
only. PX4 owns failsafes.
"""

from drone_ota.gate import check_gate, format_gate_line, format_lesson_ota, load_gate_config
from drone_ota.health import detect_platform, evaluate_health, load_health_config, tensorrt_status
from drone_ota.manifest import canonical_manifest, verify_bundle, verify_mission
from drone_ota.slots import SlotState, boot_staged, commit_pending, install_idle, note_health

__all__ = [
    "SlotState",
    "boot_staged",
    "canonical_manifest",
    "check_gate",
    "commit_pending",
    "detect_platform",
    "evaluate_health",
    "format_gate_line",
    "format_lesson_ota",
    "install_idle",
    "load_gate_config",
    "load_health_config",
    "note_health",
    "tensorrt_status",
    "verify_bundle",
    "verify_mission",
]
