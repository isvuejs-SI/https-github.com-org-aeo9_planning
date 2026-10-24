#include "aeo9/failsafe.hpp"

#include <gtest/gtest.h>

TEST(Failsafe, Latches) {
  aeo9::FailsafeMachine m;
  EXPECT_EQ(m.request(aeo9::FailsafeState::HOLD), aeo9::FailsafeState::HOLD);
  EXPECT_EQ(m.request(aeo9::FailsafeState::NOMINAL), aeo9::FailsafeState::HOLD);
  EXPECT_EQ(m.request(aeo9::FailsafeState::RTL), aeo9::FailsafeState::RTL);
  EXPECT_EQ(m.request(aeo9::FailsafeState::LAND), aeo9::FailsafeState::LAND);
}
