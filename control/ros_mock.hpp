#pragma once
// In-process stand-in for the ROS 2 supervisor node. No rclcpp.
// Subscribes to BatteryState and Image stand-ins, publishes supervisor status,
// and mocks the camera trigger. Image bytes are not stored.
#include <cstdio>
#include <string>

struct RosMock {
    int soc, mv, frame, close, conf;
    std::string status, reason;
    int triggers, fails;
    RosMock() : soc(0), mv(0), frame(0), close(0), conf(0), status("NOMINAL"), reason("ok"), triggers(0), fails(0) {}

    std::string on_battery(int s, int m) {
        soc = s;
        mv = m;
        char b[64];
        std::snprintf(b, sizeof b, "battery %d %d", s, m);
        return b;
    }
    std::string on_image(int fr, int cl, int cf) {
        frame = fr;
        close = cl;
        conf = cf;
        char b[64];
        std::snprintf(b, sizeof b, "image %d %d %d", fr, cl, cf);
        return b;
    }
    std::string publish(const std::string& st, const std::string& why) {
        status = st;
        reason = why;
        return "status " + st + " " + why;
    }
    std::string trigger(int ack) {
        triggers++;
        if (ack) return "ACK";
        fails++;
        return "NACK";
    }
};
