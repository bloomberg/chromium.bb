// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/time.h"

#include <chrono>

namespace openscreen {
namespace platform {

TimeDelta GetMonotonicTimeNow() {
  return TimeDelta::FromMicroseconds(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

TimeDelta GetUTCNow() {
  return GetMonotonicTimeNow();
}

}  // namespace platform
}  // namespace openscreen
