# Shared verdict table. Does not need ROS 2 or PX4.
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O2
PARAMS := config/params.yaml
SERVICE := systemd/drone-safety-ops.service
TABLE := build/safety_ops_table

.PHONY: all test sitl clean

all: $(TABLE)

$(TABLE): test/table_main.cpp include/drone_safety_ops/*.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Iinclude test/table_main.cpp -o $(TABLE)

test: $(TABLE)
	PYTHONPATH=. python3 -m drone_safety_ops.table $(PARAMS) $(SERVICE) > /tmp/drone_safety_py.out
	./$(TABLE) $(PARAMS) $(SERVICE) > /tmp/drone_safety_cpp.out
	diff -u /tmp/drone_safety_py.out /tmp/drone_safety_cpp.out
	diff -u test/golden_table.txt /tmp/drone_safety_cpp.out
	PYTHONPATH=. python3 test/test_package.py
	@status=0; PYTHONPATH=. python3 test/sitl/test_px4_sitl.py || status=$$?; \
	  if [ $$status -eq 77 ]; then echo "PX4 SITL: not run. This is not a pass."; \
	  elif [ $$status -ne 0 ]; then exit $$status; else echo "PX4 SITL: ran"; fi
	@if python3 -c "import launch_testing" 2>/dev/null; then \
	    echo "launch_testing is installed; run colcon test to execute it"; \
	  else echo "launch_testing: not run (package not installed). This is not a pass."; fi

sitl:
	PYTHONPATH=. python3 test/sitl/test_px4_sitl.py

clean:
	rm -rf build
