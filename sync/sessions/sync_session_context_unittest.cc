// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session_context.h"

#include "sync/internal_api/public/syncable/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace sessions {
TEST(SyncSessionContextTest, AddUnthrottleTimeTest) {
  const syncable::ModelTypeSet types(
      syncable::BOOKMARKS, syncable::PASSWORDS);

  SyncSessionContext context;
  base::TimeTicks now = base::TimeTicks::Now();
  context.SetUnthrottleTime(types, now);

  EXPECT_EQ(context.unthrottle_times_.size(), 2U);
  EXPECT_EQ(context.unthrottle_times_[syncable::BOOKMARKS], now);
  EXPECT_EQ(context.unthrottle_times_[syncable::PASSWORDS], now);
}

TEST(SyncSessionContextTest, GetCurrentlyThrottledTypesTest) {
  const syncable::ModelTypeSet types(
      syncable::BOOKMARKS, syncable::PASSWORDS);

  SyncSessionContext context;
  base::TimeTicks now = base::TimeTicks::Now();

  // Now update the throttled types with time set to 10 seconds earlier from
  // now.
  context.SetUnthrottleTime(types, now - base::TimeDelta::FromSeconds(10));
  context.PruneUnthrottledTypes(base::TimeTicks::Now());
  EXPECT_TRUE(context.GetThrottledTypes().Empty());

  // Now update the throttled types with time set to 2 hours from now.
  context.SetUnthrottleTime(types, now + base::TimeDelta::FromSeconds(1200));
  context.PruneUnthrottledTypes(base::TimeTicks::Now());
  EXPECT_TRUE(context.GetThrottledTypes().Equals(types));
}
}  // namespace sessions.
}  // namespace browser_sync

