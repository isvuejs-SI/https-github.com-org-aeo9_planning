#include "aeo9/energy.hpp"

#include <gtest/gtest.h>

TEST(Energy, StepAndLaunchGate) {
  EXPECT_EQ(aeo9::apply_energy_step(100, 200, 10, 5), 95);
  EXPECT_EQ(aeo9::apply_energy_step(5, 200, 10, 0), 0);
  EXPECT_EQ(aeo9::apply_energy_step(195, 200, 0, 20), 200);
  auto ok = aeo9::launch_gate(150, 80, 50);
  EXPECT_TRUE(ok.first);
  auto blocked = aeo9::launch_gate(100, 80, 50);
  EXPECT_FALSE(blocked.first);
}
