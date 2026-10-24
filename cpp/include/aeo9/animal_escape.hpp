#pragma once

#include <cstdint>
#include <string>

namespace aeo9 {

class AnimalEscapeController {
 public:
  AnimalEscapeController(int32_t climb_cm, int32_t stop_after)
      : climb_cm_(climb_cm), stop_after_(stop_after) {}

  std::string on_detection(bool confident, bool close);
  std::string dump_line() const;
  bool filming_allowed() const { return filming_allowed_; }

 private:
  int32_t climb_cm_;
  int32_t stop_after_;
  int32_t escape_count_{0};
  bool filming_allowed_{true};
  int32_t last_climb_cm_{0};
};

}  // namespace aeo9
