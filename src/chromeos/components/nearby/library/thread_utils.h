// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_THREAD_UTILS_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_THREAD_UTILS_H_

#include <cstdint>

#include "chromeos/components/nearby/library/exception.h"

namespace location {
namespace nearby {

class ThreadUtils {
 public:
  virtual ~ThreadUtils() {}

  // https://docs.oracle.com/javase/7/docs/api/java/lang/Thread.html#sleep(long)
  virtual Exception::Value sleep(
      std::int64_t millis) = 0;  // throws Exception::INTERRUPTED
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_THREAD_UTILS_H_
