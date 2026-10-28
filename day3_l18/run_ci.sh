#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 aeo9_day3.py budget_py.csv > py.txt
g++ -std=c++17 -O2 -Wall -Wextra aeo9_day3.cpp -o aeo9_day3
./aeo9_day3 budget_cpp.csv > cpp.txt
diff cpp.txt py.txt
cmp budget_cpp.csv budget_py.csv
diff py.txt golden.txt
grep -q "TOTAL pass=7/7" py.txt
echo "CI OK"
