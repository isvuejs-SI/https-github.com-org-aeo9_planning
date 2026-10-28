// C++ twin of sim/run_stack.py. Simulation only.
#include "sim/engine.hpp"
#include <string>

int main(int argc, char** argv) {
    std::string csv = argc > 1 ? argv[1] : "budget.csv";
    return run_stack(csv, "../config/aeo9.yaml");
}
