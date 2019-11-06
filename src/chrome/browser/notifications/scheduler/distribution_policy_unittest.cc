// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/distribution_policy.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

int MaxToShow(DistributionPolicy* policy,
              SchedulerTaskTime task_start_time,
              int quota) {
  DCHECK(policy);
  return policy->MaxToShow(task_start_time, quota);
}

TEST(DistributionPolicyTest, EvenDistributionHigherMorning) {
  auto policy = DistributionPolicy::Create();

  EXPECT_EQ(MaxToShow(policy.get(), SchedulerTaskTime::kMorning, 5 /* quota */),
            3);
  EXPECT_EQ(MaxToShow(policy.get(), SchedulerTaskTime::kMorning, 4 /* quota */),
            2);

  // Evening task should flush all remaining quota.
  EXPECT_EQ(MaxToShow(policy.get(), SchedulerTaskTime::kEvening, 5 /* quota */),
            5);
  EXPECT_EQ(MaxToShow(policy.get(), SchedulerTaskTime::kEvening, 4 /* quota */),
            4);

  // Test 0 quota.
  EXPECT_EQ(MaxToShow(policy.get(), SchedulerTaskTime::kMorning, 0 /* quota */),
            0);
  EXPECT_EQ(MaxToShow(policy.get(), SchedulerTaskTime::kMorning, 0 /* quota */),
            0);
}

}  // namespace
}  // namespace notifications
