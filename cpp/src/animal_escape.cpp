#include "aeo9/animal_escape.hpp"

namespace aeo9 {

std::string AnimalEscapeController::on_detection(bool confident, bool close) {
  if (!(confident && close)) return "ANIMAL_NONE";
  escape_count_ += 1;
  last_climb_cm_ = climb_cm_;
  if (escape_count_ >= stop_after_) {
    filming_allowed_ = false;
    return "ANIMAL_ESCAPE count=" + std::to_string(escape_count_) +
           " climb_cm=" + std::to_string(climb_cm_) + " filming=0";
  }
  return "ANIMAL_ESCAPE count=" + std::to_string(escape_count_) +
         " climb_cm=" + std::to_string(climb_cm_) + " filming=1";
}

std::string AnimalEscapeController::dump_line() const {
  return "animal escapes=" + std::to_string(escape_count_) +
         " filming=" + std::to_string(filming_allowed_ ? 1 : 0) +
         " last_climb_cm=" + std::to_string(last_climb_cm_);
}

}  // namespace aeo9
