#pragma once

#include <string>

namespace aeo9 {

enum class FailsafeState { NOMINAL, HOLD, RTL, LAND };

class FailsafeMachine {
 public:
  FailsafeState request(FailsafeState target);
  FailsafeState state() const { return state_; }
  std::string dump_line() const;

 private:
  FailsafeState state_{FailsafeState::NOMINAL};
};

const char* failsafe_name(FailsafeState s);

}  // namespace aeo9
