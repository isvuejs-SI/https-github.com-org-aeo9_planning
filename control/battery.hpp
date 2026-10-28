#pragma once
// In flight: LAND at the floor, RTL when E <= reserve + return cost.
#include <string>

inline std::string battery_action(int soc, int reserve, int ret_cost, int floor) {
    if (soc <= floor) return "LAND";
    if (soc <= reserve + ret_cost) return "RTL";
    return "OK";
}
