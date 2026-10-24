#include "aeo9/animal_escape.hpp"
#include "aeo9/config.hpp"
#include "aeo9/failsafe.hpp"
#include "aeo9/geofence.hpp"
#include "aeo9/go_nogo.hpp"
#include "aeo9/low_battery.hpp"
#include "aeo9/mission.hpp"
#include "aeo9/px4_adapter.hpp"
#include "aeo9/ros2_bridge.hpp"

#include <gtest/gtest.h>

TEST(Scenarios, NominalGo) {
  const aeo9::Config cfg = aeo9::load_config("config/aeo9.yaml");
  const aeo9::DayPlan plan = aeo9::plan_day(cfg);
  const auto d = aeo9::evaluate_go_nogo(500, cfg.max_wind_cm_s, cfg.capacity_mwh,
                                        plan.total_energy_mwh, cfg.reserve_energy_mwh,
                                        true, true);
  EXPECT_EQ(d.result, aeo9::GateResult::GO);
  aeo9::MockPx4Adapter px4;
  EXPECT_EQ(px4.arm(), "PX4_ARM");
}

TEST(Scenarios, GustWait) {
  const aeo9::Config cfg = aeo9::load_config("config/aeo9.yaml");
  const auto d = aeo9::evaluate_go_nogo(cfg.max_wind_cm_s + 1, cfg.max_wind_cm_s,
                                        cfg.capacity_mwh, 1, 1, true, true);
  EXPECT_EQ(d.result, aeo9::GateResult::WAIT);
}

TEST(Scenarios, LowBatteryAndAnimalAndCamera) {
  const aeo9::Config cfg = aeo9::load_config("config/aeo9.yaml");
  auto low = aeo9::should_return_low_battery(
      cfg.reserve_energy_mwh + cfg.return_leg_mwh, cfg.reserve_energy_mwh,
      cfg.return_leg_mwh);
  EXPECT_TRUE(low.first);

  aeo9::AnimalEscapeController animal(cfg.animal_escape_climb_cm,
                                      cfg.animal_escape_stop_after);
  for (int i = 0; i < cfg.animal_escape_stop_after; ++i) {
    animal.on_detection(true, true);
  }
  EXPECT_FALSE(animal.filming_allowed());

  aeo9::MockRos2Bridge ros;
  ros.publish_image(640, 480, 0, false);
  EXPECT_EQ(ros.trigger_camera(), "CAMERA_TRIGGER_FAIL");
  EXPECT_EQ(ros.set_starlink(false), "STARLINK=DOWN");

  aeo9::Geofence fence(0, 0, cfg.geofence_radius_cm);
  EXPECT_EQ(fence.check(cfg.geofence_radius_cm + 50, 0), aeo9::GeofenceStatus::BREACH);
}
