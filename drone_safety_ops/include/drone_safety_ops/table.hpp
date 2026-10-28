#pragma once

#include "drone_safety_ops/aggregate.hpp"
#include "drone_safety_ops/alert.hpp"
#include "drone_safety_ops/day3.hpp"
#include "drone_safety_ops/verdict_log.hpp"
#include "drone_safety_ops/watchdog.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

namespace drone_safety {

inline std::string fmt_num(double x) {
  if (std::fabs(x - std::round(x)) < 1e-9) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.0f", x);
    return buf;
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.1f", x);
  return buf;
}

inline std::string fmt_gate_line(const std::string& name, double need, const std::string& verdict, const std::string& why) {
  char buf[512];
  std::snprintf(buf, sizeof(buf), "%-26s need %6.1f Wh -> %-13s %s", name.c_str(), need, verdict.c_str(), why.c_str());
  return buf;
}

inline std::string fmt_labeled(const char* key, const Labeled& v) {
  return std::string(key) + " " + fmt_num(v.value) + " " + v.origin;
}

inline Cache parked_ready(const Params& p) {
  Cache c;
  c.soc_pct = 100;
  c.cell_temp_c = 25;
  c.pack_wh = p.mission_gate.default_pack_wh;
  c.airborne = false;
  c.armed = false;
  c.cover_open = false;
  c.wind_ms = 3;
  c.rain = false;
  c.wind_ok = true;
  c.gps_ok = true;
  c.inside_fence = true;
  c.alt_m = 0;
  c.dist_home_km = 0;
  c.legal_area_ok = true;
  c.geofence_ok = true;
  c.remote_id_ok = true;
  c.observer_ok = true;
  c.phase = "LAUNCH";
  c.day = 1;
  c.solar_forecast_wh = 0;
  c.link_gap_s = 0;
  c.detector_ok = true;
  c.lidar_kind = "none";
  c.thermal_kind = "none";
  c.props_ok = true;
  c.intrusion = false;
  c.wh_per_km = p.mission_gate.wh_per_km;
  return c;
}

inline Cache survey_green(const Params& p) {
  Cache c = parked_ready(p);
  c.airborne = true;
  c.armed = true;
  c.soc_pct = 80;
  c.cell_temp_c = 30;
  c.wind_ms = 4;
  c.alt_m = 40;
  c.dist_home_km = 4;
  c.phase = "TRANSIT";
  c.link_gap_s = 2;
  return c;
}

inline std::string snapshot_line(const std::string& name, const Cache& c, const Params& p) {
  auto s = evaluate(c, p);
  std::ostringstream os;
  os << "sensor " << name << " worst=" << s.worst_module << ":" << s.worst << " px4=" << s.px4.wire
     << " latch=" << (s.px4.latch_no_arm ? 1 : 0) << " supervisor=" << s.supervisor << " deploy=" << s.deploy
     << " gate=" << s.gate << ":" << s.gate_why << " guard=" << s.guard << " landing=" << s.landing
     << " landing_votes=" << (s.landing_votes ? 1 : 0) << " route=" << s.route;
  return os.str();
}

inline std::string watchdog_roundtrip() {
  char path[] = "/tmp/dso-wd-cpp";
  ::unlink(path);
  int fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
  if (fd < 0) return "watchdog_socket fail";
  sockaddr_un un {};
  un.sun_family = AF_UNIX;
  std::snprintf(un.sun_path, sizeof(un.sun_path), "%s", path);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&un), sizeof(un)) != 0) {
    ::close(fd);
    return "watchdog_socket fail";
  }
  setenv("NOTIFY_SOCKET", path, 1);
  const bool ok = notify_watchdog();
  char buf[64] = {};
  timeval tv {};
  tv.tv_sec = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
  ::close(fd);
  ::unlink(path);
  unsetenv("NOTIFY_SOCKET");
  if (!ok || n < 0) return "watchdog_socket fail";
  buf[n] = 0;
  if (std::string(buf) != "WATCHDOG=1") return "watchdog_socket fail";
  return "watchdog_socket ok";
}

inline std::string render_table(const std::string& params_path, const std::string& service_path) {
  const Params p = load_params(params_path);
  std::ostringstream o;

  o << "== params ==\n";
  o << fmt_labeled("landing.leg_km", p.landing.leg_km) << "\n";
  o << fmt_labeled("landing.batt_endurance_min", p.landing.batt_endurance_min) << "\n";
  o << fmt_labeled("landing.cruise_kmh", p.landing.cruise_kmh) << "\n";
  o << fmt_labeled("route.leg_km", p.route.leg_km) << "\n";
  o << fmt_labeled("route.batt_endurance_min", p.route.batt_endurance_min) << "\n";
  o << fmt_labeled("route.cruise_kmh", p.route.cruise_kmh) << "\n";
  o << "alert.holdback_s " << fmt_num(p.alert.duplicate_holdback_s) << "\n";
  o << "mission.max_days " << p.mission_gate.max_days << "\n";
  o << "mission.link_rtl_s " << fmt_num(p.mission_gate.link_rtl_s) << "\n";
  o << "mission.link_abort_s " << fmt_num(p.mission_gate.link_abort_s) << "\n";
  const std::string wd = systemd_watchdog_sec(service_path);
  o << "systemd_watchdog_sec " << wd
    << (wd == fmt_num(p.watchdog.systemd_watchdog_sec) ? " match" : " mismatch") << "\n";

  o << "== supervisor ==\n";
  const SupervisorState rows[] = {
      {"preflight, Remote ID fail", false, false, false, true, true, true, false, 95, 25, 3, false, 0, false},
      {"survey all green", true, true, true, true, true, true, false, 80, 30, 4, false, 40, false},
      {"detector timed out", true, true, true, true, true, false, false, 78, 30, 4, false, 40, false},
      {"animal threat", true, true, true, true, true, true, true, 75, 30, 4, false, 15, false},
      {"animal + geofence breach", true, true, true, true, false, true, true, 70, 30, 4, false, 15, false},
      {"low battery 22%", true, true, true, true, true, true, false, 22, 35, 4, false, 30, false},
      {"cell overtemp 65C", true, true, true, true, true, true, false, 60, 65, 4, false, 30, false},
      {"GPS lost", true, true, true, false, true, true, false, 60, 30, 4, false, 30, false},
      {"descending at 3 m", true, true, true, true, true, true, false, 40, 30, 4, false, 3, false},
      {"landed but still armed", false, true, true, true, true, true, false, 40, 30, 4, false, 0, false},
      {"landed, disarmed, windy", false, false, true, true, true, true, false, 40, 30, 14, false, 0, false},
      {"landed, disarmed, calm", false, false, true, true, true, true, false, 40, 30, 3, false, 0, false},
      {"cover open, calm", false, false, true, true, true, true, false, 40, 30, 3, false, 0, true},
  };
  for (const auto& s : rows) {
    o << s.scene << " -> " << action_name(decide(s, p.supervisor)) << " | deploy "
      << deploy_name(deploy(s, p.supervisor)) << "\n";
  }

  o << "== mission_gate ==\n";
  const MissionState missions[] = {
      {"D1 launch, park area", "LAUNCH", 1, false, true, true, 100, 400, 0, 25, 0, 0, 3, 25, false},
      {"D1 launch, all green", "LAUNCH", 1, true, true, true, 100, 400, 0, 25, 0, 0, 3, 25, false},
      {"D1 transit, healthy", "TRANSIT", 1, true, true, true, 80, 400, 4, 25, 0, 2, 5, 30, false},
      {"D2 filming, reserve hit", "FILMING", 2, true, true, true, 45, 400, 6, 25, 0, 1, 4, 32, false},
      {"D2 filming, link 45 s", "FILMING", 2, true, true, true, 70, 400, 3, 25, 0, 45, 4, 30, false},
      {"D2 filming, observer lost", "FILMING", 2, true, false, true, 70, 400, 3, 25, 0, 1, 4, 30, false},
      {"D2 night, good sun", "OVERNIGHT", 2, true, true, true, 30, 400, 8, 25, 300, 900, 4, 20, false},
      {"D3 night, cloudy", "OVERNIGHT", 3, true, true, true, 30, 400, 8, 25, 40, 900, 4, 20, false},
      {"D3 night, intrusion", "OVERNIGHT", 3, true, true, true, 60, 400, 8, 25, 300, 0, 3, 20, true},
      {"D4 transit, link lost", "TRANSIT", 4, true, true, true, 90, 400, 5, 25, 0, 700, 4, 30, false},
      {"D5 return, hot cell", "RETURN", 5, true, true, true, 60, 400, 2, 25, 0, 1, 4, 64, false},
      {"D6 still out", "RETURN", 6, true, true, true, 90, 400, 2, 25, 0, 1, 4, 30, false},
  };
  for (const auto& m : missions) {
    std::string why;
    auto v = gate(m, p.mission_gate, why);
    o << fmt_gate_line(m.name, return_wh(m, p.mission_gate), gate_name(v), why) << "\n";
  }

  o << "== day3 ==\n";
  o << render_day3(p);

  o << "== guard ==\n";
  const Detection guards[] = {
      {"none", 0, 0, 80, true, true},   {"animal", 25, 1, 80, true, true}, {"animal", 10, 0, 80, true, true},
      {"animal", 6, 1.0, 80, true, true}, {"animal", 6, 1.0, 20, true, true}, {"animal", 6, 1.0, 80, false, true},
      {"human", 12, 0.5, 80, true, true}, {"human", 3, 0, 80, true, true},
  };
  for (const auto& g : guards) {
    o << g.kind << " " << fmt_num(g.dist) << " " << fmt_num(g.closing) << " " << fmt_num(g.soc) << " "
      << (g.cover_open ? 1 : 0) << " " << (g.wind_ok ? 1 : 0) << " -> " << guard_decide(g, p.guard) << "\n";
  }

  o << "== alert ==\n";
  double clock = 1000;
  Alerter alerter(p.alert.duplicate_holdback_s, [&] { return clock; });
  for (const char* v : {"STAY", "WATCH", "WATCH", "DETER", "ESCAPE"}) {
    std::string line;
    if (alerter.alert(v, std::string(v) + ": animal 6 m, closing", true, &line)) o << line << "\n";
  }

  o << "== alert_holdback ==\n";
  double clock2 = 2000;
  Alerter hold(p.alert.duplicate_holdback_s, [&] { return clock2; });
  auto ping = [&](const char* v) {
    std::string line;
    bool sent = hold.alert(v, "again", true, &line);
    o << "t" << fmt_num(clock2) << " " << v << " " << (sent ? "true" : "false") << "\n";
  };
  ping("WATCH");
  ping("WATCH");
  clock2 = 2059;
  ping("WATCH");
  clock2 = 2060;
  ping("WATCH");
  ping("STAY");
  std::string lock_line;
  if (hold.alert("PROP_LOCKOUT", "person 3 m", true, &lock_line)) o << lock_line << "\n";

  o << "== landing ==\n";
  struct LegCase {
    double soc, cell, leg;
    bool props, geo, rid;
  };
  const LegCase legs[] = {{1.00, 30, 44.5, true, true, true}, {1.00, 30, 5, true, true, true},
                          {0.97, 50, 5, true, true, true},    {0.60, 30, 5, true, true, true},
                          {1.00, 30, 5, false, true, true},   {1.00, 30, 44.5, true, true, false}};
  for (const auto& c : legs) {
    auto r = next_leg(c.soc, c.cell, c.props, c.geo, c.rid, p.landing, c.leg);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s %.1f %.1f %.0f", r.verdict.c_str(), r.need_min, r.have_min, r.req_wh_per_h);
    o << buf << "\n";
  }

  o << "== route ==\n";
  const double straight = p.route.straight_line_mi * p.route.mi_to_km;
  struct RouteCase {
    bool real, tribal, waiver, park;
  };
  const RouteCase routes[] = {{false, false, false, true}, {true, false, false, true}, {true, true, false, true},
                              {true, true, true, true},    {false, false, false, false}};
  for (const auto& c : routes) {
    auto r = plan_route(straight, p.route, c.real, c.tribal, c.waiver, c.park);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s %.1f %d %.0f", r.verdict.c_str(), r.one_pack_km, r.legs, r.req);
    o << buf << "\n";
  }

  o << "== px4 ==\n";
  struct MapCase {
    const char* verdict;
    bool air;
  };
  const MapCase maps[] = {{"CONTINUE", true}, {"HOLD", true}, {"CLIMB_ESCAPE", true}, {"RTL", true},
                          {"LAND", true},     {"NO_TAKEOFF", false}, {"PROP_LOCKOUT", false}, {"PROP_LOCKOUT", true},
                          {"ESCAPE", false},  {"NO_GO", false}, {"NO_GO", true}, {"ABORT_MISSION", true},
                          {"ABORT_MISSION", false}, {"LAND_NOW", true}, {"HOLD_GROUND", false}, {"DETER", false},
                          {"GO_SIM", false}, {"STAY", false}, {"WATCH", false}};
  for (const auto& c : maps) {
    auto cmd = map_request(c.verdict, c.air);
    o << "map " << c.verdict << " " << (c.air ? "air" : "ground") << " -> " << cmd.wire << " latch="
      << (cmd.latch_no_arm ? 1 : 0) << "\n";
  }
  o << "cmd " << command_signature(fill_command("hold", false)) << "\n";
  o << "cmd " << command_signature(fill_command("rtl", false)) << "\n";
  o << "cmd " << command_signature(fill_command("land", false)) << "\n";
  o << "cmd " << command_signature(fill_command("no_arm", true)) << "\n";
  for (const char* wire : {"hold", "rtl", "land", "no_arm"}) {
    o << "mavsdk " << wire << " " << mavsdk_action(wire) << "\n";
  }
  auto combo = combine_requests({map_request("PROP_LOCKOUT", true), map_request("RTL", true)}, true);
  o << "combine air PROP_LOCKOUT+RTL -> " << combo.wire << " latch=" << (combo.latch_no_arm ? 1 : 0) << "\n";

  o << "== log ==\n";
  VerdictLog log("/tmp/drone_safety_ops_table_cpp");
  const char* ts = "2026-01-01T00:00:00Z";
  for (auto rec : {log.append(ts, "guard", "STAY", "none", ""),
                   log.append(ts, "guard", "PROP_LOCKOUT", "no_arm", "person 3 m"),
                   log.append(ts, "guard", "PROP_LOCKOUT", "no_arm", "person 3 m")}) {
    o << format_log_record(rec) << "\n";
  }

  o << "== watchdog ==\n";
  o << watchdog_roundtrip() << "\n";

  o << "== json ==\n";
  auto js = parse_json("{\"soc_pct\":22.5,\"airborne\":true,\"kind\":\"human\"}");
  o << "soc " << fmt_num(js.num("soc_pct")) << " airborne " << (js.boolean("airborne") ? 1 : 0) << " kind " << js.str("kind")
    << "\n";

  o << "== sensors ==\n";
  o << snapshot_line("unconfigured", Cache{}, p) << "\n";
  {
    auto c = parked_ready(p);
    c.remote_id_ok = false;
    o << snapshot_line("parked_remote_id_fail", c, p) << "\n";
  }
  o << snapshot_line("survey_green", survey_green(p), p) << "\n";
  {
    auto c = survey_green(p);
    c.soc_pct = 22;
    c.cell_temp_c = 35;
    o << snapshot_line("air_low_battery", c, p) << "\n";
  }
  {
    auto c = survey_green(p);
    c.link_gap_s = 700;
    c.soc_pct = 90;
    c.dist_home_km = 5;
    o << snapshot_line("air_link_lost", c, p) << "\n";
  }
  {
    auto c = parked_ready(p);
    c.lidar_kind = "human";
    c.lidar_dist = 3;
    c.lidar_closing = 0;
    o << snapshot_line("parked_person_3m", c, p) << "\n";
  }
  {
    auto c = survey_green(p);
    c.lidar_kind = "human";
    c.lidar_dist = 3;
    o << snapshot_line("air_person_3m", c, p) << "\n";
  }
  {
    auto c = parked_ready(p);
    c.phase = "OVERNIGHT";
    c.soc_pct = 35;
    c.dist_home_km = 8;
    c.solar_forecast_wh = 40;
    c.link_gap_s = 900;
    c.wind_ms = 14;
    c.lidar_kind = "human";
    c.lidar_dist = 6;
    c.intrusion = true;
    o << snapshot_line("overnight_intrusion", c, p) << "\n";
  }
  return o.str();
}

}  // namespace drone_safety
