#include "aeo9/config.hpp"

#include <gtest/gtest.h>

TEST(Config, LoadsLimits) {
  const aeo9::Config cfg = aeo9::load_config("config/aeo9.yaml");
  EXPECT_EQ(cfg.seed, 42);
  EXPECT_TRUE(cfg.sim_only);
  EXPECT_EQ(cfg.max_wind_cm_s, 1200);
  EXPECT_EQ(cfg.reserve_energy_mwh, 50000);
  EXPECT_EQ(cfg.geofence_radius_cm, 50000);
  EXPECT_EQ(cfg.min_altitude_agl_cm, 3000);
  EXPECT_EQ(cfg.home_name, "tusayan_staging");
  EXPECT_LT(cfg.home_lat_e7, 360500000);
}
