#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 stack_test.py > py.txt
g++ -std=c++17 -O2 -Wall -Wextra -I.. stack_test.cpp -o stack_test
./stack_test > cpp.txt
diff cpp.txt py.txt
diff py.txt golden.txt
grep -q "TOTAL pass=83/83" py.txt
echo "CI OK"
