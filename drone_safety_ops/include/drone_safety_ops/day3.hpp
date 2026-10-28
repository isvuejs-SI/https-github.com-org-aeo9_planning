#pragma once

#include "drone_safety_ops/mission_gate.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace drone_safety {

struct Day3Row {
  const char* t;
  const char* label;
  const char* phase;
  int day;
  bool legal;
  bool observer;
  bool remote_id;
  double soc;
  double pack;
  double dist;
  double wh;
  double solar;
  double gap;
  double wind;
  double cell;
  bool intrusion;
  const char* note;
};

inline const std::vector<Day3Row>& day3_rows() {
  static const std::vector<Day3Row> rows = {
      {"06:30", "PREFLIGHT", "LAUNCH", 3, true, true, true, 90, 400, 8, 25, 0, 0, 4, 22, false,
       "after D2 overnight, SoC 90%"},
      {"07:15", "TRANSIT", "TRANSIT", 3, true, true, true, 78, 400, 5, 25, 0, 2, 5, 28, false, "en route to filming site"},
      {"08:40", "FILMING", "FILMING", 3, true, true, true, 70, 400, 6, 25, 0, 1, 5, 31, false,
       "animal: supervisor CLIMB_ESCAPE"},
      {"09:30", "FILMING", "FILMING", 3, true, true, true, 64, 400, 4, 25, 0, 45, 5, 30, false, "Starlink/MAVLink gap 45 s"},
      {"09:31", "FILMING", "FILMING", 3, true, true, true, 63, 400, 4, 25, 0, 2, 5, 30, false, "link restored"},
      {"13:30", "RETURN_CHK", "FILMING", 3, true, true, true, 48, 400, 6, 25, 0, 1, 6, 33, false, "afternoon reserve check"},
      {"19:00", "OVERNIGHT", "OVERNIGHT", 3, true, true, true, 35, 400, 8, 25, 40, 900, 4, 20, false, "cloudy forecast 40 Wh"},
      {"22:10", "OVERNIGHT", "OVERNIGHT", 3, true, true, true, 35, 400, 8, 25, 40, 900, 14, 18, true,
       "gust 14 m/s + intrusion"},
  };
  return rows;
}

inline std::string render_day3(const Params& p) {
  std::string out = "AEO-9 DAY 3 (SIM, placeholder numbers; legal area = outside GRCA NP, VLOS + observer)\n";
  for (const auto& r : day3_rows()) {
    MissionState m;
    m.name = r.label;
    m.phase = r.phase;
    m.day = r.day;
    m.legal_area_ok = r.legal;
    m.observer_ok = r.observer;
    m.remote_id_ok = r.remote_id;
    m.soc_pct = r.soc;
    m.pack_wh = r.pack;
    m.dist_home_km = r.dist;
    m.wh_per_km = r.wh;
    m.solar_forecast_wh = r.solar;
    m.link_gap_s = r.gap;
    m.wind_ms = r.wind;
    m.cell_temp_c = r.cell;
    m.intrusion = r.intrusion;
    std::string why;
    auto v = gate(m, p.mission_gate, why);
    char buf[768];
    std::snprintf(buf, sizeof(buf),
                  "%s %-10s SoC%3.0f%% home%3.0fkm gap%4.0fs wind%3.0f sun%4.0f need%6.1fWh -> %-13s %s | %s", r.t,
                  r.label, r.soc, r.dist, r.gap, r.wind, r.solar, return_wh(m, p.mission_gate), gate_name(v), why.c_str(),
                  r.note);
    out += buf;
    out += "\n";
  }
  return out;
}

}  // namespace drone_safety
