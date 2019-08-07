// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/clock.h"

namespace openscreen {

platform::TimeDelta PlatformClock::Now() {
  return platform::GetMonotonicTimeNow();
}

}  // namespace openscreen
