#include "aeo9/geofence.hpp"

#include <gtest/gtest.h>

TEST(Geofence, BreachLatches) {
  aeo9::Geofence fence(0, 0, 50000);
  EXPECT_EQ(fence.check(1000, 1000), aeo9::GeofenceStatus::INSIDE);
  EXPECT_EQ(fence.check(50100, 0), aeo9::GeofenceStatus::BREACH);
  EXPECT_EQ(fence.check(0, 0), aeo9::GeofenceStatus::BREACH);
}
