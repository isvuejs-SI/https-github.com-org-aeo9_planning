#include "aeo9/failsafe.hpp"

namespace aeo9 {
namespace {

int rank(FailsafeState s) {
  switch (s) {
    case FailsafeState::NOMINAL:
      return 0;
    case FailsafeState::HOLD:
      return 1;
    case FailsafeState::RTL:
      return 2;
    case FailsafeState::LAND:
      return 3;
  }
  return 0;
}

}  // namespace

const char* failsafe_name(FailsafeState s) {
  switch (s) {
    case FailsafeState::NOMINAL:
      return "NOMINAL";
    case FailsafeState::HOLD:
      return "HOLD";
    case FailsafeState::RTL:
      return "RTL";
    case FailsafeState::LAND:
      return "LAND";
  }
  return "NOMINAL";
}

FailsafeState FailsafeMachine::request(FailsafeState target) {
  if (rank(target) >= rank(state_)) {
    state_ = target;
  }
  return state_;
}

std::string FailsafeMachine::dump_line() const {
  return std::string("failsafe=") + failsafe_name(state_);
}

}  // namespace aeo9
