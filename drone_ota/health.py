"""Boot health check. TensorRT is stubbed when nvcc or a GPU is absent."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from drone_ota.yaml_subset import load_yaml

CONFIG_PATH = Path(__file__).resolve().parent / "config" / "ota_gate.yaml"


@dataclass
class HealthConfig:
    boot_loop_rollback_threshold: int
    max_time_offset_ms: float
    node_timeout_s: float
    heartbeat_timeout_s: float
    sensor_timeout_s: float
    health_window_s: int
    tensorrt_required: bool
    required_nodes: list[str]
    required_sensors: list[str]
    hardware_id: str


@dataclass
class Observation:
    nodes: list[str]
    heartbeat_age_s: float
    sensor_age_s: dict[str, float]
    nvme_writable: bool
    time_offset_ms: float
    boot_failures: int = 0


@dataclass
class Platform:
    nvcc: bool = False
    gpu: bool = False
    cuda_linked: bool = False


def load_health_config(path: str | None = None) -> HealthConfig:
    data = load_yaml(str(path or CONFIG_PATH))
    health = data["health"]
    return HealthConfig(
        boot_loop_rollback_threshold=int(health["boot_loop_rollback_threshold"]),
        max_time_offset_ms=float(health["max_time_offset_ms"]),
        node_timeout_s=float(health["node_timeout_s"]),
        heartbeat_timeout_s=float(health["heartbeat_timeout_s"]),
        sensor_timeout_s=float(health["sensor_timeout_s"]),
        health_window_s=int(health["health_window_s"]),
        tensorrt_required=bool(health["tensorrt_required"]),
        required_nodes=list(health["required_nodes"]),
        required_sensors=list(health["required_sensors"]),
        hardware_id=str(data["hardware"]["id"]),
    )


def tensorrt_status(platform: Platform) -> str:
    # The device image would call cudaGetDeviceCount and load the engine.
    # This host module is not linked to CUDA and does not report PASS unless
    # a test injects cuda_linked.
    if not platform.nvcc or not platform.gpu:
        return "SKIP no_nvcc_or_gpu"
    if not platform.cuda_linked:
        return "SKIP cuda_runtime_not_linked"
    return "PASS"


def detect_platform() -> Platform:
    path = os.environ.get("PATH", "")
    nvcc = any(os.path.isfile(os.path.join(directory, "nvcc")) for directory in path.split(":") if directory)
    gpu = os.path.exists("/dev/nvidia0") or os.path.exists("/dev/nvidiactl")
    return Platform(nvcc=nvcc, gpu=gpu, cuda_linked=False)


def evaluate_health(obs: Observation, config: HealthConfig, platform: Platform) -> str:
    present = set(obs.nodes)
    missing_nodes = sorted(name for name in config.required_nodes if name not in present)
    heartbeat_ok = not (obs.heartbeat_age_s > config.heartbeat_timeout_s)
    missing_sensors = sorted(
        name
        for name in config.required_sensors
        if name not in obs.sensor_age_s or obs.sensor_age_s[name] > config.sensor_timeout_s
    )
    tensorrt = tensorrt_status(platform)
    time_ok = not (obs.time_offset_ms > config.max_time_offset_ms or obs.time_offset_ms < -config.max_time_offset_ms)
    tensorrt_ok = tensorrt == "PASS" or (tensorrt.startswith("SKIP") and not config.tensorrt_required)
    failed = bool(missing_nodes or not heartbeat_ok or missing_sensors or not obs.nvme_writable or not time_ok or not tensorrt_ok)
    if not failed:
        boot_out = 0
        action = "OK"
    else:
        boot_out = obs.boot_failures + 1
        action = "ROLLBACK" if boot_out >= config.boot_loop_rollback_threshold else "RETRY"
    nodes = "PASS" if not missing_nodes else "FAIL missing=" + "+".join(missing_nodes)
    sensors = "PASS" if not missing_sensors else "FAIL missing=" + "+".join(missing_sensors)
    lines = [
        f"nodes={nodes}",
        f"mavlink_heartbeat={'PASS' if heartbeat_ok else 'FAIL'}",
        f"sensors={sensors}",
        f"tensorrt={tensorrt}",
        f"nvme={'PASS' if obs.nvme_writable else 'FAIL'}",
        f"time_sync={'PASS' if time_ok else 'FAIL'}",
        f"boot_failures={boot_out}",
        f"action={action}",
    ]
    return "\n".join(lines) + "\n"
