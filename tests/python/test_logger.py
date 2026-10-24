"""AEO-15: CSV run logger under logs/."""

from pathlib import Path

from sim.logger import CsvRunLogger

ROOT = Path(__file__).resolve().parents[2]


def test_csv_logger_writes_gitignored_logs(tmp_path: Path):
    path = tmp_path / "run.csv"
    # Prefer writing under repo logs/ when not using tmp — structure check:
    assert "logs/" in (ROOT / ".gitignore").read_text(encoding="utf-8")

    logger = CsvRunLogger(path)
    logger.log(0, "NOMINAL", 100000, 0)
    logger.log(1000, "RTL", 80000, 3000)
    out = logger.flush()
    text = out.read_text(encoding="utf-8")
    assert text.splitlines()[0] == "time_ms,state,energy_mwh,altitude_cm"
    assert "RTL" in text
    assert logger.row_count == 2
