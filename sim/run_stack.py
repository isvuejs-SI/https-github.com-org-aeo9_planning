"""Print the seven-scenario stack trace and write the per-minute CSV."""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from planner.config_loader import load_file
from sim.engine import run_all


def main():
    cfg_path = os.path.join(ROOT, "config", "aeo9.yaml")
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "budget.csv"
    lines, rows, npass, total = run_all(load_file(cfg_path))
    print("\n".join(lines))
    with open(csv_path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(rows) + "\n")
    return 0 if npass == total else 1


if __name__ == "__main__":
    sys.exit(main())
