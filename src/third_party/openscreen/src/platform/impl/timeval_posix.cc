// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/timeval_posix.h"

#include <chrono>

namespace openscreen {

struct timeval ToTimeval(const Clock::duration& timeout) {
  struct timeval tv;
  const auto whole_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(timeout);
  tv.tv_sec = whole_seconds.count();
  tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(
                   timeout - whole_seconds)
                   .count();

  return tv;
}

}  // namespace openscreen
