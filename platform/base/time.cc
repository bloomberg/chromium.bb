#include "platform/api/time.h"

#include <chrono>

namespace openscreen {
namespace platform {

Microseconds GetMonotonicTimeNow() {
  return Microseconds(std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count());
}

}  // namespace platform
}  // namespace openscreen
