"""Typed limits loaded from YAML. Algorithms read this object."""

from __future__ import annotations

import os
from dataclasses import dataclass, fields

from drone_pc.common.yaml_subset import parse_yaml


def _need(node: dict, key: str):
    if key not in node:
        raise KeyError(key)
    return node[key]


def _f(node: dict, key: str) -> float:
    return float(_need(node, key))


def _i(node: dict, key: str) -> int:
    return int(_need(node, key))


def _s(node: dict, key: str) -> str:
    value = _need(node, key)
    if not isinstance(value, str):
        raise TypeError(key)
    return value


def _b(node: dict, key: str) -> bool:
    value = _need(node, key)
    if not isinstance(value, bool):
        raise TypeError(key)
    return value


def _polygon(seq) -> list[tuple[float, float]]:
    return [(float(point["x"]), float(point["y"])) for point in seq]


@dataclass
class Limits:
    wind_go_max_mps: float
    wind_hold_max_mps: float
    temp_no_go_low_c: float
    temp_land_now_low_c: float
    temp_hold_low_c: float
    temp_hold_high_c: float
    temp_land_now_high_c: float
    temp_no_go_high_c: float
    pack_capacity_wh: float
    wh_per_km: float
    return_distance_factor: float
    pack_reserve_fraction: float
    soc_go_min: float
    soc_hold_min: float
    soc_rtl_min: float
    escape_min_soc: float
    geofence: list[tuple[float, float]]
    no_fly: list[list[tuple[float, float]]]
    alt_min_m: float
    alt_max_m: float
    speed_max_mps: float
    speed_cruise_mps: float
    speed_survey_mps: float
    speed_takeoff_mps: float
    speed_escape_mps: float
    speed_return_mps: float
    speed_land_mps: float
    slow_radius_m: float
    lookahead_cells: int
    vlos_max_range_m: float
    heartbeat_rate_hz: float
    heartbeat_timeout_s: float
    link_rtl_after_s: float
    link_abort_after_s: float
    ntfy_base_url: str
    ntfy_topic: str
    ntfy_cooldown_s: float
    ntfy_enable_send: bool
    log_directory: str
    escape_watch_range_m: float
    escape_deter_range_m: float
    escape_escape_range_m: float
    escape_prop_lockout_range_m: float
    escape_climb_m: float
    escape_max_wind_mps: float
    grid_resolution_m: float
    grid_inflation_m: float
    grid_predict_steps: int
    grid_predict_dt_s: float
    grid_size_x: int
    grid_size_y: int
    grid_size_z: int
    grid_origin_x_m: float
    grid_origin_y_m: float
    grid_origin_z_m: float
    topic_pointcloud: str
    topic_thermal: str
    topic_link: str
    topic_px4_request: str

    def speed_for_mode(self, mode: str) -> float:
        table = {
            "takeoff": self.speed_takeoff_mps,
            "cruise": self.speed_cruise_mps,
            "survey": self.speed_survey_mps,
            "return": self.speed_return_mps,
            "escape": self.speed_escape_mps,
            "land": self.speed_land_mps,
        }
        if mode not in table:
            raise KeyError(mode)
        return table[mode]

    def scalar_map(self) -> dict:
        """JSON-friendly snapshot of every scalar limit, plus fence sizes."""
        skip = {"geofence", "no_fly"}
        data = {}
        for field in fields(self):
            if field.name in skip:
                continue
            value = getattr(self, field.name)
            if isinstance(value, bool):
                data[field.name] = value
            elif isinstance(value, int) and not isinstance(value, bool):
                data[field.name] = value
            elif isinstance(value, float):
                data[field.name] = value
            else:
                data[field.name] = value
        data["geofence_points"] = len(self.geofence)
        data["no_fly_zones"] = len(self.no_fly)
        data["geofence_first_x"] = self.geofence[0][0]
        data["geofence_first_y"] = self.geofence[0][1]
        data["no_fly_first_x"] = self.no_fly[0][0][0]
        data["no_fly_first_y"] = self.no_fly[0][0][1]
        return data


def limits_from_mapping(raw: dict) -> Limits:
    wind = _need(raw, "wind")
    temperature = _need(raw, "temperature")
    battery = _need(raw, "battery")
    geofence = _need(raw, "geofence")
    altitude = _need(raw, "altitude")
    speed = _need(raw, "speed")
    local = _need(raw, "local")
    vlos = _need(raw, "vlos")
    heartbeat = _need(raw, "heartbeat")
    link = _need(raw, "link")
    ntfy = _need(raw, "ntfy")
    logging = _need(raw, "logging")
    escape = _need(raw, "escape")
    grid = _need(raw, "grid")
    topics = _need(raw, "topics")
    no_fly_raw = _need(raw, "no_fly")
    return Limits(
        wind_go_max_mps=_f(wind, "go_max_mps"),
        wind_hold_max_mps=_f(wind, "hold_max_mps"),
        temp_no_go_low_c=_f(temperature, "no_go_low_c"),
        temp_land_now_low_c=_f(temperature, "land_now_low_c"),
        temp_hold_low_c=_f(temperature, "hold_low_c"),
        temp_hold_high_c=_f(temperature, "hold_high_c"),
        temp_land_now_high_c=_f(temperature, "land_now_high_c"),
        temp_no_go_high_c=_f(temperature, "no_go_high_c"),
        pack_capacity_wh=_f(battery, "pack_capacity_wh"),
        wh_per_km=_f(battery, "wh_per_km"),
        return_distance_factor=_f(battery, "return_distance_factor"),
        pack_reserve_fraction=_f(battery, "pack_reserve_fraction"),
        soc_go_min=_f(battery, "soc_go_min"),
        soc_hold_min=_f(battery, "soc_hold_min"),
        soc_rtl_min=_f(battery, "soc_rtl_min"),
        escape_min_soc=_f(battery, "escape_min_soc"),
        geofence=_polygon(_need(geofence, "polygon")),
        no_fly=[_polygon(_need(zone, "polygon")) for zone in no_fly_raw],
        alt_min_m=_f(altitude, "min_m"),
        alt_max_m=_f(altitude, "max_m"),
        speed_max_mps=_f(speed, "max_mps"),
        speed_cruise_mps=_f(speed, "cruise_mps"),
        speed_survey_mps=_f(speed, "survey_mps"),
        speed_takeoff_mps=_f(speed, "takeoff_mps"),
        speed_escape_mps=_f(speed, "escape_mps"),
        speed_return_mps=_f(speed, "return_mps"),
        speed_land_mps=_f(speed, "land_mps"),
        slow_radius_m=_f(speed, "slow_radius_m"),
        lookahead_cells=_i(local, "lookahead_cells"),
        vlos_max_range_m=_f(vlos, "max_range_m"),
        heartbeat_rate_hz=_f(heartbeat, "rate_hz"),
        heartbeat_timeout_s=_f(heartbeat, "timeout_s"),
        link_rtl_after_s=_f(link, "rtl_after_s"),
        link_abort_after_s=_f(link, "abort_after_s"),
        ntfy_base_url=_s(ntfy, "base_url"),
        ntfy_topic=_s(ntfy, "topic"),
        ntfy_cooldown_s=_f(ntfy, "cooldown_s"),
        ntfy_enable_send=_b(ntfy, "enable_send"),
        log_directory=_s(logging, "directory"),
        escape_watch_range_m=_f(escape, "watch_range_m"),
        escape_deter_range_m=_f(escape, "deter_range_m"),
        escape_escape_range_m=_f(escape, "escape_range_m"),
        escape_prop_lockout_range_m=_f(escape, "prop_lockout_range_m"),
        escape_climb_m=_f(escape, "climb_m"),
        escape_max_wind_mps=_f(escape, "max_wind_mps"),
        grid_resolution_m=_f(grid, "resolution_m"),
        grid_inflation_m=_f(grid, "inflation_m"),
        grid_predict_steps=_i(grid, "predict_steps"),
        grid_predict_dt_s=_f(grid, "predict_dt_s"),
        grid_size_x=_i(grid, "size_x"),
        grid_size_y=_i(grid, "size_y"),
        grid_size_z=_i(grid, "size_z"),
        grid_origin_x_m=_f(grid, "origin_x_m"),
        grid_origin_y_m=_f(grid, "origin_y_m"),
        grid_origin_z_m=_f(grid, "origin_z_m"),
        topic_pointcloud=_s(topics, "pointcloud"),
        topic_thermal=_s(topics, "thermal"),
        topic_link=_s(topics, "link"),
        topic_px4_request=_s(topics, "px4_request"),
    )


def default_limits_path() -> str:
    env = os.environ.get("DRONE_PC_LIMITS")
    if env:
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.abspath(os.path.join(here, "..", "config", "limits.yaml"))


def load_limits(path: str | None = None) -> Limits:
    target = path or default_limits_path()
    with open(target, "r", encoding="utf-8") as handle:
        raw = parse_yaml(handle.read())
    if not isinstance(raw, dict):
        raise ValueError("limits file must be a mapping")
    return limits_from_mapping(raw)
