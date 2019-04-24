// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

TEST(CooperativeSchedulingManager, WhitelistedStackScope) {
  std::unique_ptr<CooperativeSchedulingManager> manager =
      std::make_unique<CooperativeSchedulingManager>();
  {
    EXPECT_FALSE(manager->InWhitelistedStackScope());
    CooperativeSchedulingManager::WhitelistedStackScope scope(manager.get());
    EXPECT_TRUE(manager->InWhitelistedStackScope());
    {
      CooperativeSchedulingManager::WhitelistedStackScope nested_scope(
          manager.get());
      EXPECT_TRUE(manager->InWhitelistedStackScope());
    }
    EXPECT_TRUE(manager->InWhitelistedStackScope());
  }
  EXPECT_FALSE(manager->InWhitelistedStackScope());
}

}  // namespace scheduler
}  // namespace blink
