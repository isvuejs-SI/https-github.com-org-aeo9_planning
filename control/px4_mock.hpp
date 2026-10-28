#pragma once
// PX4 request interface. Mock only: no socket, no MAVSDK, no live MAVLink.
// The supervisor may request arm, takeoff, goto, hold, return, or land.
// PX4 keeps its own failsafes; nothing here clears them.
#include <string>

struct Px4Mock {
    std::string mode;
    int armed;
    int x, y, z;
    int requests;
    Px4Mock() : mode("DISARMED"), armed(0), x(0), y(0), z(0), requests(0) {}

    std::string connect(const std::string& url) {
        (void)url;
        return "REFUSE simulation_only";
    }
    std::string arm() {
        requests++;
        armed = 1;
        mode = "ARMED";
        return "ACK";
    }
    std::string takeoff(int alt) {
        requests++;
        if (!armed) return "REFUSE not_armed";
        mode = "TAKEOFF";
        z = alt;
        return "ACK";
    }
    std::string go_to(int nx, int ny, int nz) {
        requests++;
        if (!armed) return "REFUSE not_armed";
        mode = "GOTO";
        x = nx;
        y = ny;
        z = nz;
        return "ACK";
    }
    std::string hold() {
        requests++;
        if (!armed) return "REFUSE not_armed";
        mode = "HOLD";
        return "ACK";
    }
    std::string do_return() {
        requests++;
        if (!armed) return "REFUSE not_armed";
        mode = "RETURN";
        return "ACK";
    }
    std::string land() {
        requests++;
        mode = "LAND";
        z = 0;
        armed = 0;
        return "ACK";
    }
};
