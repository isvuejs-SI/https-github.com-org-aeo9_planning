"""Load config/params.yaml. Labeled numbers keep origin: relayed or assumed."""

from dataclasses import dataclass
import os

import yaml


@dataclass
class Labeled:
    value: float
    origin: str


@dataclass
class SupervisorParams:
    batt_no_takeoff_pct: float
    cell_temp_limit_c: float
    batt_rtl_pct: float
    batt_land_pct: float
    gear_down_alt_m: float
    deploy_wind_max_ms: float
    detector_timeout_s: float


@dataclass
class MissionParams:
    reserve_fraction: float
    energy_margin: float
    link_rtl_s: float
    link_abort_s: float
    max_days: int
    overnight_wind_hold_ms: float
    flight_wind_rtl_ms: float
    cell_overtemp_c: float
    wh_per_km: float
    default_pack_wh: float


@dataclass
class GuardParams:
    human_lockout_m: float
    watch_beyond_m: float
    animal_escape_within_m: float
    animal_closing_mps: float
    escape_min_soc_pct: float


@dataclass
class LandingParams:
    leg_km: Labeled
    batt_endurance_min: Labeled
    cruise_kmh: Labeled
    reserve_fraction: float
    energy_margin: float
    cell_hold_c: float
    soc_hold_fraction: float


@dataclass
class RouteParams:
    straight_line_mi: float
    mi_to_km: float
    leg_km: Labeled
    batt_endurance_min: Labeled
    cruise_kmh: Labeled
    reserve_fraction: float
    energy_margin: float
    real_flight: bool
    tribal_permission: bool
    faa_waiver: bool


@dataclass
class AlertParams:
    duplicate_holdback_s: float
    ntfy_host: str
    default_topic: str


@dataclass
class WatchdogParams:
    period_s: float
    systemd_watchdog_sec: float


@dataclass
class LoggingParams:
    nvme_dir: str
    fallback_dir: str


@dataclass
class Px4Params:
    transport: str
    uxrce_command_topic: str
    request_topic: str
    no_arm_latch_topic: str


@dataclass
class Params:
    supervisor: SupervisorParams
    mission_gate: MissionParams
    guard: GuardParams
    landing: LandingParams
    route: RouteParams
    alert: AlertParams
    watchdog: WatchdogParams
    logging: LoggingParams
    px4: Px4Params


def _labeled(node):
    if isinstance(node, dict) and "value" in node:
        return Labeled(float(node["value"]), str(node.get("origin", "")))
    return Labeled(float(node), "")


def package_root():
    return os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def default_params_path():
    env = os.environ.get("DRONE_SAFETY_PARAMS")
    if env:
        return env
    return os.path.join(package_root(), "config", "params.yaml")


def load_params(path=None):
    path = path or default_params_path()
    with open(path, "r", encoding="utf-8") as handle:
        raw = yaml.safe_load(handle)
    sup = raw["supervisor"]
    gate = raw["mission_gate"]
    guard = raw["guard"]
    landing = raw["landing"]
    route = raw["route"]
    alert = raw["alert"]
    watchdog = raw["watchdog"]
    logging = raw["logging"]
    px4 = raw["px4"]
    return Params(
        supervisor=SupervisorParams(
            float(sup["batt_no_takeoff_pct"]),
            float(sup["cell_temp_limit_c"]),
            float(sup["batt_rtl_pct"]),
            float(sup["batt_land_pct"]),
            float(sup["gear_down_alt_m"]),
            float(sup["deploy_wind_max_ms"]),
            float(sup["detector_timeout_s"]),
        ),
        mission_gate=MissionParams(
            float(gate["reserve_fraction"]),
            float(gate["energy_margin"]),
            float(gate["link_rtl_s"]),
            float(gate["link_abort_s"]),
            int(gate["max_days"]),
            float(gate["overnight_wind_hold_ms"]),
            float(gate["flight_wind_rtl_ms"]),
            float(gate["cell_overtemp_c"]),
            float(gate["wh_per_km"]),
            float(gate["default_pack_wh"]),
        ),
        guard=GuardParams(
            float(guard["human_lockout_m"]),
            float(guard["watch_beyond_m"]),
            float(guard["animal_escape_within_m"]),
            float(guard["animal_closing_mps"]),
            float(guard["escape_min_soc_pct"]),
        ),
        landing=LandingParams(
            _labeled(landing["leg_km"]),
            _labeled(landing["batt_endurance_min"]),
            _labeled(landing["cruise_kmh"]),
            float(landing["reserve_fraction"]),
            float(landing["energy_margin"]),
            float(landing["cell_hold_c"]),
            float(landing["soc_hold_fraction"]),
        ),
        route=RouteParams(
            float(route["straight_line_mi"]),
            float(route["mi_to_km"]),
            _labeled(route["leg_km"]),
            _labeled(route["batt_endurance_min"]),
            _labeled(route["cruise_kmh"]),
            float(route["reserve_fraction"]),
            float(route["energy_margin"]),
            bool(route["real_flight"]),
            bool(route["tribal_permission"]),
            bool(route["faa_waiver"]),
        ),
        alert=AlertParams(
            float(alert["duplicate_holdback_s"]),
            str(alert["ntfy_host"]),
            str(alert["default_topic"]),
        ),
        watchdog=WatchdogParams(float(watchdog["period_s"]), float(watchdog["systemd_watchdog_sec"])),
        logging=LoggingParams(str(logging["nvme_dir"]), str(logging["fallback_dir"])),
        px4=Px4Params(
            str(px4["transport"]),
            str(px4["uxrce_command_topic"]),
            str(px4["request_topic"]),
            str(px4["no_arm_latch_topic"]),
        ),
    )
