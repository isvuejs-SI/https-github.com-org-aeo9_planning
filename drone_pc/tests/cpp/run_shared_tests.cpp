#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "drone_pc/control/link_loss.hpp"
#include "drone_pc/control/px4_interface.hpp"
#include "drone_pc/params.hpp"
#include "drone_pc/planning/energy_planner.hpp"
#include "drone_pc/planning/escape_planner.hpp"
#include "drone_pc/planning/global_planner.hpp"
#include "drone_pc/planning/local_planner.hpp"
#include "drone_pc/planning/occupancy_map.hpp"
#include "drone_pc/planning/state_machine.hpp"
#include "drone_pc/safety/alerts.hpp"
#include "drone_pc/safety/event_logger.hpp"
#include "drone_pc/safety/preflight_gate.hpp"
#include "drone_pc/third_party/json.hpp"

using json = nlohmann::json;
using namespace drone_pc;

#ifndef DRONE_PC_TEST_DIR
#define DRONE_PC_TEST_DIR "tests/shared"
#endif

static double r3(double x) { return std::round(x * 1000.0) / 1000.0; }

static Point3 pt(const json& arr) {
  return Point3{arr[0].get<double>(), arr[1].get<double>(), arr[2].get<double>()};
}

static json run_case(const json& case_j, const Params& params) {
  const std::string mod = case_j.at("module").get<std::string>();
  const json& inp = case_j.at("input");

  if (mod == "state_machine") {
    StateMachine sm(mission_phase_from_string(inp.at("initial").get<std::string>()));
    json oks = json::array();
    for (const auto& step : inp.at("steps")) {
      bool ok = sm.transition(mission_phase_from_string(step.at("to").get<std::string>()),
                              step.at("reason").get<std::string>());
      oks.push_back(ok);
    }
    json reasons = json::array();
    for (const auto& h : sm.history()) reasons.push_back(h.reason);
    return json{{"final", to_string(sm.current())}, {"ok", oks}, {"reasons", reasons}};
  }

  if (mod == "preflight") {
    PreflightGate gate(params);
    PreflightInputs in;
    in.wind_mps = inp.at("wind_mps").get<double>();
    in.cell_temp_c = inp.at("cell_temp_c").get<double>();
    in.soc_percent = inp.at("soc_percent").get<double>();
    in.geofence_ok = inp.at("geofence_ok").get<bool>();
    in.remote_id_ok = inp.at("remote_id_ok").get<bool>();
    in.return_reserve_ok = inp.at("return_reserve_ok").get<bool>();
    in.airborne = inp.at("airborne").get<bool>();
    auto res = gate.evaluate(in);
    json reasons = json::array();
    for (const auto& r : res.reasons) reasons.push_back(r);
    return json{{"verdict", to_string(res.verdict)}, {"reasons", reasons}};
  }

  if (mod == "energy") {
    EnergyPlanner planner(params);
    EnergyPlanInput in;
    in.distance_to_home_km = inp.at("distance_to_home_km").get<double>();
    in.soc_percent = inp.at("soc_percent").get<double>();
    in.remaining_mission_km = inp.at("remaining_mission_km").get<double>();
    in.overnight = inp.at("overnight").get<bool>();
    in.current_position = pt(inp.at("current"));
    for (const auto& b : inp.at("backups")) in.candidate_backup_sites.push_back(pt(b));
    auto res = planner.evaluate(in);
    return json{{"return_reserve_wh", r3(res.return_reserve_wh)},
                {"available_wh", r3(res.available_wh)},
                {"return_reserve_ok", res.return_reserve_ok},
                {"stay_overnight", res.stay_overnight},
                {"backup_selected", res.backup_selected},
                {"backup", json::array({r3(res.backup_site.x), r3(res.backup_site.y),
                                        r3(res.backup_site.z)})},
                {"reason", res.reason}};
  }

  if (mod == "escape_classify") {
    EscapePlanner planner(params);
    json verdicts = json::array();
    for (const auto& d : inp.at("distances_m")) {
      verdicts.push_back(to_string(planner.classify_threat(d.get<double>())));
    }
    return json{{"verdicts", verdicts}};
  }

  if (mod == "escape") {
    EscapePlanner planner(params);
    EscapeInput in;
    in.threat_verdict = escape_verdict_from_string(inp.at("threat").get<std::string>());
    in.soc_percent = inp.at("soc_percent").get<double>();
    in.cover_open = inp.at("cover_open").get<bool>();
    in.wind_ok = inp.at("wind_ok").get<bool>();
    in.current_position = pt(inp.at("current"));
    in.backup_site = pt(inp.at("backup"));
    in.backup_valid = inp.at("backup_valid").get<bool>();
    auto res = planner.plan(in);
    return json{{"action", to_string(res.action)},
                {"execute_escape", res.execute_escape},
                {"target",
                 json::array({r3(res.target.x), r3(res.target.y), r3(res.target.z)})},
                {"reason", res.reason}};
  }

  if (mod == "link_loss") {
    LinkLossMonitor mon(params);
    json actions = json::array();
    json reasons = json::array();
    for (const auto& step : inp.at("steps")) {
      auto res = mon.update(step.at("dt_s").get<double>(), step.at("link_ok").get<bool>());
      actions.push_back(to_string(res.action));
      reasons.push_back(res.reason);
    }
    return json{{"actions", actions}, {"reasons", reasons}};
  }

  if (mod == "alerts") {
    AlertManager am(params);
    json sent = json::array();
    json reasons = json::array();
    for (const auto& ev : inp.at("events")) {
      auto rec = am.maybe_send(ev.at("key").get<std::string>(),
                               ev.at("message").get<std::string>(),
                               ev.at("t").get<double>());
      sent.push_back(rec.sent);
      reasons.push_back(rec.reason);
    }
    return json{{"sent", sent}, {"reasons", reasons}};
  }

  if (mod == "px4") {
    Px4Interface iface(params);
    for (const auto& name : inp.at("requests")) {
      const std::string n = name.get<std::string>();
      if (n == "ARM") iface.request(Px4Request::ARM);
      else if (n == "TAKEOFF") iface.request(Px4Request::TAKEOFF);
      else if (n == "HOLD") iface.request(Px4Request::HOLD);
      else if (n == "RTL") iface.request(Px4Request::RTL);
      else if (n == "LAND") iface.request(Px4Request::LAND);
    }
    iface.tick(inp.at("heartbeat_gap_s").get<double>());
    json log = json::array();
    for (const auto& e : iface.request_log()) log.push_back(e);
    return json{{"log", log},
                {"heartbeat_ok", iface.status().heartbeat_ok},
                {"fallback", iface.status().fallback}};
  }

  if (mod == "occupancy") {
    OccupancyMap map(params);
    std::vector<Point3> points;
    for (const auto& p : inp.at("points")) points.push_back(pt(p));
    map.integrate_points(points);
    map.integrate_points(points);
    std::vector<MovingObstacle> moving;
    for (const auto& m : inp.at("moving")) {
      MovingObstacle o;
      o.position = pt(m.at("pos"));
      o.label = m.at("label").get<std::string>();
      o.radius_m = m.at("radius_m").get<double>();
      moving.push_back(o);
    }
    map.add_moving_obstacles(moving);
    json occupied = json::array();
    for (const auto& c : inp.at("check")) occupied.push_back(map.occupied_world(pt(c)));
    return json{{"occupied", occupied}};
  }

  if (mod == "global_planner") {
    OccupancyMap map(params);
    for (const auto& b : inp.at("block")) {
      int ix, iy, iz;
      if (map.world_to_index(pt(b), ix, iy, iz)) {
        // Force-occupy via moving obstacle at cell center
        MovingObstacle o;
        o.position = map.index_to_world(ix, iy, iz);
        o.radius_m = 0.1;
        map.add_moving_obstacles({o});
      }
    }
    GlobalPlanner planner(params);
    auto res = planner.plan(map, pt(inp.at("start")), pt(inp.at("goal")));
    json first = json::array({0.0, 0.0, 0.0});
    json last = json::array({0.0, 0.0, 0.0});
    if (!res.path.empty()) {
      first = json::array({r3(res.path.front().x), r3(res.path.front().y), r3(res.path.front().z)});
      last = json::array({r3(res.path.back().x), r3(res.path.back().y), r3(res.path.back().z)});
    }
    return json{{"success", res.success},
                {"reason", res.reason},
                {"path_len", static_cast<int>(res.path.size())},
                {"first", first},
                {"last", last}};
  }

  if (mod == "local_planner") {
    OccupancyMap map(params);
    LocalPlanner planner(params);
    LocalPlanInput in;
    in.position = pt(inp.at("position"));
    in.goal = pt(inp.at("goal"));
    in.home = pt(inp.at("home"));
    auto res = planner.plan(map, in);
    return json{{"stop", res.stop}, {"reason", res.reason}, {"speed_mps", r3(res.speed_mps)}};
  }

  if (mod == "logger") {
    EventLogger logger(params);
    for (const auto& v : inp.at("verdicts")) {
      logger.log_verdict(v.at("t").get<double>(), v.at("payload").get<std::string>());
    }
    for (const auto& t : inp.at("transitions")) {
      logger.log_transition(t.at("t").get<double>(), t.at("payload").get<std::string>());
    }
    json cats = json::array();
    json payloads = json::array();
    for (const auto& e : logger.events()) {
      cats.push_back(e.category);
      payloads.push_back(e.payload);
    }
    return json{{"categories", cats}, {"payloads", payloads}};
  }

  throw std::runtime_error("unknown module: " + mod);
}

static bool match_expect(const json& case_j, const json& out) {
  const json& exp = case_j.at("expect");
  const std::string mod = case_j.at("module").get<std::string>();
  if (mod == "global_planner") {
    return out.at("success") == exp.at("success") && out.at("reason") == exp.at("reason") &&
           out.at("path_len").get<int>() >= exp.at("path_len_min").get<int>() &&
           out.at("first") == json::array({r3(exp.at("first")[0].get<double>()),
                                           r3(exp.at("first")[1].get<double>()),
                                           r3(exp.at("first")[2].get<double>())}) &&
           out.at("last") == json::array({r3(exp.at("last")[0].get<double>()),
                                          r3(exp.at("last")[1].get<double>()),
                                          r3(exp.at("last")[2].get<double>())});
  }
  if (mod == "energy" || mod == "escape" || mod == "local_planner") {
    json exp_n = exp;
    if (exp_n.contains("return_reserve_wh"))
      exp_n["return_reserve_wh"] = r3(exp_n["return_reserve_wh"].get<double>());
    if (exp_n.contains("available_wh"))
      exp_n["available_wh"] = r3(exp_n["available_wh"].get<double>());
    if (exp_n.contains("backup"))
      exp_n["backup"] = json::array({r3(exp_n["backup"][0].get<double>()),
                                     r3(exp_n["backup"][1].get<double>()),
                                     r3(exp_n["backup"][2].get<double>())});
    if (exp_n.contains("target"))
      exp_n["target"] = json::array({r3(exp_n["target"][0].get<double>()),
                                     r3(exp_n["target"][1].get<double>()),
                                     r3(exp_n["target"][2].get<double>())});
    if (exp_n.contains("speed_mps")) exp_n["speed_mps"] = r3(exp_n["speed_mps"].get<double>());
    return out == exp_n;
  }
  return out == exp;
}

int main(int argc, char** argv) {
  std::string test_dir = DRONE_PC_TEST_DIR;
  if (argc > 1) test_dir = argv[1];
  const std::string cases_path = test_dir + "/test_cases.json";
  std::ifstream in(cases_path);
  if (!in) {
    std::cerr << "failed to open " << cases_path << "\n";
    return 2;
  }
  json data;
  in >> data;

  Params params = Params::defaults();
  json results = json::array();
  int failed = 0;
  for (const auto& case_j : data.at("cases")) {
    json out = run_case(case_j, params);
    const bool ok = match_expect(case_j, out);
    results.push_back(json{{"id", case_j.at("id")}, {"pass", ok}, {"output", out}});
    std::cout << "[" << (ok ? "PASS" : "FAIL") << "] " << case_j.at("id").get<std::string>()
              << "\n";
    if (!ok) {
      ++failed;
      std::cout << "  expected=" << case_j.at("expect") << "\n";
      std::cout << "  actual  =" << out << "\n";
    }
  }

  const std::string out_path = test_dir + "/cpp_results.json";
  std::ofstream out(out_path);
  out << std::setw(2) << results << "\n";
  std::cout << "cpp: " << (results.size() - failed) << "/" << results.size() << " passed\n";
  return failed ? 1 : 0;
}
