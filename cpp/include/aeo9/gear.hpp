#pragma once

#include <string>
#include <utility>

namespace aeo9 {

enum class GearActionResult { OK, REFUSED };

class GearController {
 public:
  std::pair<GearActionResult, std::string> stow();
  std::pair<GearActionResult, std::string> arm();
  std::pair<GearActionResult, std::string> deploy();
  std::pair<GearActionResult, std::string> land();
  std::string dump_line() const;

 private:
  bool stowed_{true};
  bool armed_{false};
  bool landed_{true};
};

}  // namespace aeo9
