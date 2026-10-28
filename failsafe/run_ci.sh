#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 failsafe_sim.py > py.txt
g++ -std=c++17 -O2 -Wall -Wextra failsafe_sim.cpp -o failsafe_sim
./failsafe_sim > cpp.txt
diff cpp.txt py.txt                      # twins must match byte for byte
grep -q "TOTAL pass=10/10" py.txt        # every scenario criterion holds
echo "CI OK"
