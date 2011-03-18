#pragma once
#include <string>

namespace debug {
class System {
 public:
  static std::string GetLastErrorDescription(int error_code = 0);
};
}  // namespace debug