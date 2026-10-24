"""AEO-2: config loader tests."""

from pathlib import Path

from planner.config_loader import load_config

ROOT = Path(__file__).resolve().parents[2]
CFG = ROOT / "config" / "aeo9.yaml"


def test_load_limits_from_yaml():
    cfg = load_config(CFG)
    assert cfg.seed == 42
    assert cfg.sim_only is True
    assert cfg.max_wind_cm_s == 1200
    assert cfg.reserve_energy_mwh == 50000
    assert cfg.geofence_radius_cm == 50000
    assert cfg.min_altitude_agl_cm == 3000


def test_home_outside_grand_canyon_np():
    """Staging home is south of GCNP (unauthorized park flights banned)."""
    cfg = load_config(CFG)
    assert cfg.home_name == "tusayan_staging"
    # Rough park south rim ~36.05N; staging ~35.88N stays south.
    assert cfg.home_lat_e7 < 360500000
    assert cfg.home_lon_e7 < 0
