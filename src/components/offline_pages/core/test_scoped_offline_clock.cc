// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/test_scoped_offline_clock.h"

#include "components/offline_pages/core/offline_clock.h"

namespace offline_pages {

TestScopedOfflineClock::TestScopedOfflineClock() {
  SetOfflineClockForTesting(this);
}
TestScopedOfflineClock::~TestScopedOfflineClock() {
  SetOfflineClockForTesting(nullptr);
}

}  // namespace offline_pages
