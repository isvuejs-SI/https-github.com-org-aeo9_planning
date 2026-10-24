#include <gtest/gtest.h>

#include <filesystem>

TEST(Skeleton, RequiredPathsExist) {
  EXPECT_TRUE(std::filesystem::is_directory("planner"));
  EXPECT_TRUE(std::filesystem::is_directory("control"));
  EXPECT_TRUE(std::filesystem::is_directory("sim"));
  EXPECT_TRUE(std::filesystem::is_directory("tests"));
  EXPECT_TRUE(std::filesystem::is_directory("config"));
  EXPECT_TRUE(std::filesystem::is_regular_file("config/aeo9.yaml"));
  EXPECT_TRUE(std::filesystem::is_regular_file("run_ci.sh"));
}
