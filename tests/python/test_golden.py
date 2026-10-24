"""AEO-14: Python dump matches golden file."""

from pathlib import Path

from planner.dump import build_dump

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "tests" / "golden" / "supervisor_dump.txt"
CFG = ROOT / "config" / "aeo9.yaml"


def test_python_matches_golden():
    got = "\n".join(build_dump(CFG)) + "\n"
    expected = GOLDEN.read_text(encoding="utf-8")
    assert got == expected
