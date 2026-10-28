#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 aeo9_mission.py > py.txt
g++ -std=c++17 -O2 -Wall -Wextra aeo9_mission.cpp -o aeo9_mission
./aeo9_mission > cpp.txt
diff cpp.txt py.txt
diff py.txt golden.txt
grep -q "TOTAL pass=5/5" py.txt
echo "CI OK"
