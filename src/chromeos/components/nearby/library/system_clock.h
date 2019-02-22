// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SYSTEM_CLOCK_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SYSTEM_CLOCK_H_

#include <cstdint>

namespace location {
namespace nearby {

class SystemClock {
 public:
  virtual ~SystemClock() {}

  // Returns the time since the system was booted, and includes deep sleep. This
  // clock should be guaranteed to be monotonic, and should continue to tick
  // even when the CPU is in power saving modes.
  virtual std::int64_t elapsedRealtime() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SYSTEM_CLOCK_H_
