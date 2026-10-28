"""In flight: LAND at the floor, RTL when E <= reserve + return cost."""


def battery_action(soc, reserve, ret_cost, floor):
    if soc <= floor:
        return "LAND"
    if soc <= reserve + ret_cost:
        return "RTL"
    return "OK"
