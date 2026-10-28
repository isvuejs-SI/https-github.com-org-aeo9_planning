#include "drone_safety_ops/table.hpp"

#include <iostream>

int main(int argc, char** argv) {
  const char* params = argc > 1 ? argv[1] : "config/params.yaml";
  const char* service = argc > 2 ? argv[2] : "systemd/drone-safety-ops.service";
  try {
    std::cout << drone_safety::render_table(params, service);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  return 0;
}
