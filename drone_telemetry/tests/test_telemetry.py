"""Summary packet and the 1 kB/s budget. Same fixture as the C++ tests."""

from pathlib import Path

from drone_telemetry.summary import encode_summary, load_telemetry_config, within_budget

ROOT = Path(__file__).resolve().parents[1]
CONFIG = load_telemetry_config(str(ROOT / "config" / "telemetry.yaml"))


def test_packets_and_budget():
    assert CONFIG.max_bytes_per_sec == 1024
    assert CONFIG.publish_hz == 1
    assert CONFIG.temp_warn_c == 70.0
    assert CONFIG.temp_fail_c == 80.0
    assert CONFIG.nvme_free_warn_pct == 15.0
    assert CONFIG.nvme_wear_warn_pct == 80.0
    count = 0
    for line in (ROOT / "tests" / "fixtures" / "packets.txt").read_text(encoding="utf-8").splitlines():
        if not line:
            continue
        fields = line.split("|")
        packet = encode_summary(
            fields[0],
            float(fields[1]),
            float(fields[2]),
            float(fields[3]),
            float(fields[4]),
            fields[5] == "1",
            fields[6],
            CONFIG,
        )
        assert packet == fields[7] + "\n"
        assert within_budget(len(packet.encode("utf-8")), CONFIG.publish_hz, CONFIG.max_bytes_per_sec)
        count += 1
    assert count == 5
    assert within_budget(1023, 1, 1024)
    assert not within_budget(1024, 1, 1024)
    mode = "M" * (CONFIG.max_field_len + 8)
    fail = "N" * (CONFIG.max_field_len + 4)
    wide = encode_summary(mode, 50, 20, 50, 1, False, fail, CONFIG)
    assert f"mode={'M' * CONFIG.max_field_len} " in wide
    assert wide.endswith(f"failsafe={'N' * CONFIG.max_field_len}\n")
    assert within_budget(len(wide.encode("utf-8")), CONFIG.publish_hz, CONFIG.max_bytes_per_sec)
    assert len(wide.encode("utf-8")) * CONFIG.publish_hz < CONFIG.max_bytes_per_sec
