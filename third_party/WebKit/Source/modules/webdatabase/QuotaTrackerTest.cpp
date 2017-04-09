// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webdatabase/QuotaTracker.h"

#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace {

TEST(QuotaTrackerTest, UpdateAndGetSizeAndSpaceAvailable) {
  QuotaTracker& tracker = QuotaTracker::Instance();
  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("file:///a/b/c");

  const unsigned long long kSpaceAvailable = 12345678ULL;
  tracker.UpdateSpaceAvailableToOrigin(origin.Get(), kSpaceAvailable);

  const String database_name = "db";
  const unsigned long long kDatabaseSize = 1234ULL;
  tracker.UpdateDatabaseSize(origin.Get(), database_name, kDatabaseSize);

  unsigned long long used = 0;
  unsigned long long available = 0;
  tracker.GetDatabaseSizeAndSpaceAvailableToOrigin(origin.Get(), database_name,
                                                   &used, &available);

  EXPECT_EQ(used, kDatabaseSize);
  EXPECT_EQ(available, kSpaceAvailable);
}

TEST(QuotaTrackerTest, LocalAccessBlocked) {
  QuotaTracker& tracker = QuotaTracker::Instance();
  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("file:///a/b/c");

  const unsigned long long kSpaceAvailable = 12345678ULL;
  tracker.UpdateSpaceAvailableToOrigin(origin.Get(), kSpaceAvailable);

  const String database_name = "db";
  const unsigned long long kDatabaseSize = 1234ULL;
  tracker.UpdateDatabaseSize(origin.Get(), database_name, kDatabaseSize);

  // QuotaTracker should not care about policy, just identity.
  origin->BlockLocalAccessFromLocalOrigin();

  unsigned long long used = 0;
  unsigned long long available = 0;
  tracker.GetDatabaseSizeAndSpaceAvailableToOrigin(origin.Get(), database_name,
                                                   &used, &available);

  EXPECT_EQ(used, kDatabaseSize);
  EXPECT_EQ(available, kSpaceAvailable);
}

}  // namespace
}  // namespace blink
