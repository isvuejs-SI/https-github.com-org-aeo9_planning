#include "aeo9/animal_escape.hpp"
#include "aeo9/config.hpp"
#include "aeo9/energy.hpp"
#include "aeo9/failsafe.hpp"
#include "aeo9/gear.hpp"
#include "aeo9/geofence.hpp"
#include "aeo9/go_nogo.hpp"
#include "aeo9/low_battery.hpp"
#include "aeo9/mission.hpp"
#include "aeo9/px4_adapter.hpp"
#include "aeo9/ros2_bridge.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> build_dump(const std::string& config_path) {
  const aeo9::Config cfg = aeo9::load_config(config_path);
  std::vector<std::string> lines;
  lines.push_back("AEO9_SUPERVISOR_V1");
  for (const auto& line : aeo9::config_dump_lines(cfg)) lines.push_back(line);

  const int32_t e0 = cfg.capacity_mwh / 2;
  const int32_t e1 = aeo9::apply_energy_step(
      e0, cfg.capacity_mwh, cfg.hover_draw_mwh_per_s, cfg.solar_charge_mwh_per_s);
  lines.push_back("energy_step E=" + std::to_string(e0) +
                  " draw=" + std::to_string(cfg.hover_draw_mwh_per_s) +
                  " solar=" + std::to_string(cfg.solar_charge_mwh_per_s) +
                  " -> E=" + std::to_string(e1));

  const aeo9::DayPlan plan = aeo9::plan_day(cfg);
  for (const auto& line : aeo9::day_plan_dump_lines(plan)) lines.push_back(line);

  const auto gate = aeo9::launch_gate(e1, plan.total_energy_mwh, cfg.reserve_energy_mwh);
  lines.push_back("launch_gate ok=" + std::to_string(gate.first ? 1 : 0) + " " +
                  gate.second);

  const aeo9::GateDecision decision = aeo9::evaluate_go_nogo(
      800, cfg.max_wind_cm_s, e1, plan.total_energy_mwh, cfg.reserve_energy_mwh,
      true, true);
  lines.push_back(aeo9::gate_dump_line(decision));

  aeo9::FailsafeMachine fs;
  fs.request(aeo9::FailsafeState::HOLD);
  fs.request(aeo9::FailsafeState::NOMINAL);
  fs.request(aeo9::FailsafeState::RTL);
  lines.push_back(fs.dump_line());

  aeo9::Geofence fence(0, 0, cfg.geofence_radius_cm);
  lines.push_back(fence.dump_line(1000, 1000));
  lines.push_back(fence.dump_line(cfg.geofence_radius_cm + 100, 0));

  aeo9::AnimalEscapeController animal(cfg.animal_escape_climb_cm,
                                      cfg.animal_escape_stop_after);
  for (int i = 0; i < 3; ++i) lines.push_back(animal.on_detection(true, true));
  lines.push_back(animal.dump_line());

  const auto low = aeo9::should_return_low_battery(
      cfg.reserve_energy_mwh + cfg.return_leg_mwh, cfg.reserve_energy_mwh,
      cfg.return_leg_mwh);
  lines.push_back("low_battery rtl=" + std::to_string(low.first ? 1 : 0) + " " +
                  low.second);

  aeo9::GearController gear;
  lines.push_back(gear.arm().second);
  lines.push_back(gear.deploy().second);
  gear.land();
  lines.push_back(gear.deploy().second);
  lines.push_back(gear.stow().second);
  lines.push_back(gear.arm().second);
  lines.push_back(gear.dump_line());

  aeo9::MockPx4Adapter px4;
  lines.push_back(px4.arm());
  lines.push_back(px4.takeoff(cfg.min_altitude_agl_cm));
  lines.push_back(px4.goto_pos(1000, 2000, cfg.min_altitude_agl_cm));
  lines.push_back(px4.hold());
  lines.push_back(px4.return_to_launch());
  lines.push_back(px4.land());

  aeo9::MockRos2Bridge ros;
  lines.push_back(ros.publish_battery(90, 21000));
  lines.push_back(ros.publish_image(640, 480, 0, true));
  lines.push_back(ros.trigger_camera());
  aeo9::SupervisorStatus status;
  status.state = aeo9::failsafe_name(fs.state());
  status.energy_mwh = e1;
  status.altitude_cm = cfg.min_altitude_agl_cm;
  status.filming = animal.filming_allowed();
  lines.push_back(ros.publish_status(status));
  lines.push_back(ros.set_starlink(false));

  lines.push_back("END");
  return lines;
}

}  // namespace

int main(int argc, char** argv) {
  std::string config_path = "config/aeo9.yaml";
  if (argc > 1) config_path = argv[1];
  for (const auto& line : build_dump(config_path)) {
    std::cout << line << '\n';
  }
  return 0;
}
