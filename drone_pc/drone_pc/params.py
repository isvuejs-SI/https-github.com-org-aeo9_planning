from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class Params:
    preflight_max_wind_mps: float = 12.0
    preflight_max_cell_temp_c: float = 45.0
    preflight_min_cell_temp_c: float = 5.0
    preflight_min_soc_percent: float = 35.0
    preflight_require_remote_id: bool = True
    preflight_require_geofence_ok: bool = True
    preflight_require_return_reserve: bool = True

    energy_wh_per_km: float = 80.0
    energy_reserve_distance_factor: float = 1.3
    energy_pack_reserve_fraction: float = 0.20
    energy_pack_capacity_wh: float = 400.0
    energy_escape_min_soc_percent: float = 30.0
    energy_overnight_solar_wh_estimate: float = 120.0
    energy_min_backup_landing_clearance_m: float = 15.0

    map_resolution_m: float = 1.0
    map_size_x: int = 100
    map_size_y: int = 100
    map_size_z: int = 40
    map_origin_x_m: float = -50.0
    map_origin_y_m: float = -50.0
    map_origin_z_m: float = 0.0
    map_occupied_threshold: float = 0.65
    map_moving_obstacle_inflate_m: float = 2.0

    global_heuristic_weight: float = 1.0
    global_max_expansions: int = 200000

    local_max_speed_mps: float = 8.0
    local_min_speed_mps: float = 0.5
    local_max_altitude_m: float = 120.0
    local_min_altitude_m: float = 5.0
    local_obstacle_stop_distance_m: float = 4.0
    local_obstacle_slow_distance_m: float = 12.0
    local_vlos_max_range_m: float = 500.0
    local_avoidance_lateral_step_m: float = 3.0

    escape_climb_altitude_m: float = 25.0
    escape_watch_radius_m: float = 30.0
    escape_deter_radius_m: float = 15.0
    escape_escape_radius_m: float = 8.0
    escape_prop_lockout_radius_m: float = 3.0

    control_offboard_rate_hz: float = 20.0
    control_heartbeat_period_s: float = 0.05
    control_heartbeat_miss_limit: int = 3
    control_default_takeoff_altitude_m: float = 10.0

    link_loss_rtl_after_s: float = 30.0
    link_loss_abort_after_s: float = 600.0

    alerts_ntfy_base_url: str = "https://ntfy.sh"
    alerts_ntfy_topic: str = "drone_pc_alerts"
    alerts_duplicate_holdback_s: float = 60.0
    alerts_enabled: bool = True
    logging_log_directory: str = "/tmp/drone_pc_logs"
    logging_log_verdicts: bool = True
    logging_log_transitions: bool = True

    @staticmethod
    def defaults() -> "Params":
        return Params()
