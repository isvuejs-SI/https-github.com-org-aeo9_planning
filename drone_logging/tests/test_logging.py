"""Logging, upload queue, and bag manifest. Same fixtures as the C++ tests."""

from pathlib import Path

from drone_logging.budget import flights_capacity, rate_tenths, record_command, retention_action, usable_gb
from drone_logging.events import append_event, apply_delete, delete_decision, format_event
from drone_logging.manifest_log import manifest_meta, manifest_sha256, verify_sha256_manifest
from drone_logging.queue import UploadQueue, format_queue, link_ready, load_log_config, ordered_names

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"
LESSON = FIXTURES / "lesson"
CONFIG = load_log_config(str(ROOT / "config" / "logging.yaml"))


def _lesson(name: str) -> bytes:
    return (LESSON / name).read_bytes()


def test_events_match_fixture_and_are_kept(tmp_path):
    path = tmp_path / "events.jsonl"
    for args in (
        ("mode", 10, "2026-10-24T00:00:00Z", "AUTO", "RTL", "low_battery"),
        ("failsafe", 11, "2026-10-24T00:00:01Z", "none", "RTL", "geofence"),
        ("failsafe", 12, "2026-10-24T00:00:02Z", "none", "land", 'a "b" \\ c'),
    ):
        append_event(str(path), format_event(*args))
    expected = (FIXTURES / "events_expected.jsonl").read_bytes()
    assert path.read_bytes() == expected
    assert apply_delete(str(path), "events", "QUEUED", CONFIG.retained) == "DELETE_REFUSED unverified_flight_safety"
    assert path.read_bytes() == expected
    assert apply_delete(str(path), "events", "VERIFIED", CONFIG.retained) == "DELETE_REFUSED retained_flight_safety"
    assert delete_decision("failsafe", "QUEUED", CONFIG.retained) == "DELETE_REFUSED unverified_flight_safety"
    assert delete_decision("pixhawk", "UPLOADED", CONFIG.retained) == "DELETE_REFUSED unverified_flight_safety"
    assert delete_decision("video", "QUEUED", CONFIG.retained) == "DELETE_REFUSED unverified"
    video = tmp_path / "clip.mp4"
    video.write_bytes(b"video")
    assert apply_delete(str(video), "video", "VERIFIED", CONFIG.retained) == "DELETE_OK"
    assert not video.exists()


def test_lesson_queue_and_resume(tmp_path):
    assert CONFIG.max_retries == 3
    assert CONFIG.link_stable_s == 60
    assert link_ready(60, True, CONFIG)
    assert not link_ready(59, True, CONFIG)
    assert not link_ready(60, False, CONFIG)

    files = {name: _lesson(name) for name in (
        "events.jsonl", "failsafe.jsonl", "px4_log.ulg", "summary.md", "cam.mp4", "lidar.mcap", "notes.mcap"
    )}
    shuffled = ["cam.mp4", "summary.md", "lidar.mcap", "px4_log.ulg", "failsafe.jsonl", "events.jsonl"]
    landing = ordered_names(shuffled, CONFIG, True)
    assert format_queue(landing, files) == (FIXTURES / "lesson_queue.txt").read_text(encoding="utf-8")

    names = ["cam.mp4", "notes.mcap", "summary.md", "lidar.mcap", "px4_log.ulg", "failsafe.jsonl", "events.jsonl"]
    assert ordered_names(names, CONFIG, True) == [
        "events.jsonl", "failsafe.jsonl", "px4_log.ulg", "summary.md", "notes.mcap"
    ]
    assert ordered_names(names, CONFIG, False) == [
        "events.jsonl", "failsafe.jsonl", "px4_log.ulg", "summary.md", "notes.mcap", "cam.mp4", "lidar.mcap"
    ]

    queue = UploadQueue(CONFIG)
    queue.add("events.jsonl", b"hello-events", "starlink_landing")
    assert queue.accept_chunk("events.jsonl", 0, 5) == "RESUME"
    assert queue.dump() == (FIXTURES / "queue_state.json").read_text(encoding="utf-8")
    state = tmp_path / "queue.json"
    queue.save(str(state))
    restored = UploadQueue.load(str(state), CONFIG)
    assert restored.dump() == queue.dump()
    assert restored.accept_chunk("events.jsonl", 0, 1) == "OFFSET_MISMATCH"
    assert restored.accept_chunk("events.jsonl", 5, 7) == "COMPLETE"
    assert restored.finish("events.jsonl", restored.get("events.jsonl").sha256) == "VERIFIED"

    tight = load_log_config(str(ROOT / "config" / "logging.yaml"))
    tight.max_retries = 1
    once = UploadQueue(tight)
    once.add("summary.md", b"summary", "starlink_landing")
    assert once.finish("summary.md", "deadbeef") == "STUCK"

    retries = UploadQueue(CONFIG)
    retries.add("summary.md", b"summary", "starlink_landing")
    assert retries.finish("summary.md", "deadbeef") == "RETRY"
    assert retries.finish("summary.md", "deadbeef") == "RETRY"
    assert retries.finish("summary.md", "deadbeef") == "STUCK"
    assert retries.finish("summary.md", retries.get("summary.md").sha256) == "STUCK"


def test_manifest_budget_and_record_command():
    assert usable_gb(CONFIG) == 700
    assert flights_capacity(CONFIG) == 31
    assert rate_tenths(CONFIG) == 121
    assert CONFIG.reserve_free_percent == 20
    assert CONFIG.est_gb_per_flight == 22
    stop = CONFIG.stop_nonessential_free_percent
    delete_below = CONFIG.delete_when_free_below_percent
    assert retention_action(stop, 0, CONFIG) == "HOLD"
    assert retention_action(stop - 1, 0, CONFIG) == "STOP_NONESSENTIAL_AND_ALERT"
    assert retention_action(stop - 1, 2, CONFIG) == "DELETE_VERIFIED"
    assert retention_action(delete_below, 3, CONFIG) == "OK"
    assert retention_action(delete_below - 1, 3, CONFIG) == "DELETE_VERIFIED"
    assert retention_action(delete_below - 1, 0, CONFIG) == "HOLD"

    files = {"events.jsonl": _lesson("events.jsonl"), "summary.md": _lesson("summary.md")}
    assert manifest_sha256(files) == (FIXTURES / "MANIFEST.sha256").read_text(encoding="utf-8")
    assert manifest_meta("aeo9", "1.3.0", "2026-10-24T01:00:00Z", "2026-10-24T02:00:00Z", files) == (
        FIXTURES / "MANIFEST.json"
    ).read_text(encoding="utf-8")
    assert verify_sha256_manifest(manifest_sha256(files), files)
    tampered = dict(files)
    tampered["summary.md"] = files["summary.md"] + b"x"
    assert not verify_sha256_manifest((FIXTURES / "MANIFEST.sha256").read_text(encoding="utf-8"), tampered)

    command = record_command(CONFIG)
    assert command[:5] == ["ros2", "bag", "record", "--storage", "mcap"]
    assert command[6] == str(CONFIG.max_bag_size_mb * 1024 * 1024)
    assert command[7:] == CONFIG.topics
    launch = (ROOT / "launch" / "record_mcap.launch.py").read_text(encoding="utf-8")
    assert "mcap" in launch
    assert "generate_launch_description" in launch
    assert "not a successful bag recording" in launch
