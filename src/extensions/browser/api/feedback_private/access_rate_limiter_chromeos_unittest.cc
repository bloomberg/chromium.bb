// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/access_rate_limiter.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

class AccessRateLimiterTest : public ::testing::Test {
 public:
  AccessRateLimiterTest() {
    // |test_clock_| must start out at something other than 0, which is
    // interpreted as an invalid value.
    test_clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  }

  ~AccessRateLimiterTest() override = default;

 protected:
  // For manually testing time-sensitive behavior.
  base::SimpleTestTickClock test_clock_;

  // Unit under test.
  std::unique_ptr<extensions::AccessRateLimiter> limiter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessRateLimiterTest);
};

TEST_F(AccessRateLimiterTest, MaxAccessCountOfZero) {
  limiter_.reset(new extensions::AccessRateLimiter(
      0, base::TimeDelta::FromMilliseconds(100), &test_clock_));

  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
}

TEST_F(AccessRateLimiterTest, NormalRepeatedAccess) {
  limiter_.reset(new extensions::AccessRateLimiter(
      5, base::TimeDelta::FromMilliseconds(100), &test_clock_));

  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
}

TEST_F(AccessRateLimiterTest, RechargeWhenDry) {
  limiter_.reset(new extensions::AccessRateLimiter(
      5, base::TimeDelta::FromMilliseconds(100), &test_clock_));

  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());

  test_clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());

  test_clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
}

TEST_F(AccessRateLimiterTest, RechargeTimeOfZero) {
  limiter_.reset(new extensions::AccessRateLimiter(
      5, base::TimeDelta::FromMilliseconds(0), &test_clock_));

  // Unlimited number of accesses.
  for (int i = 0; i < 100; ++i)
    EXPECT_TRUE(limiter_->AttemptAccess()) << i;

  // Advancing should not make a difference.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());

  test_clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
}

TEST_F(AccessRateLimiterTest, RechargeToMax) {
  limiter_.reset(new extensions::AccessRateLimiter(
      5, base::TimeDelta::FromMilliseconds(100), &test_clock_));

  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());

  // Should not exceed the max number of accesses.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
}

TEST_F(AccessRateLimiterTest, IncrementalRecharge) {
  limiter_.reset(new extensions::AccessRateLimiter(
      5, base::TimeDelta::FromMilliseconds(100), &test_clock_));

  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());

  // Has not yet hit the full recharge period.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(50));
  EXPECT_FALSE(limiter_->AttemptAccess());

  // Has finally hit the full recharge period.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(50));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());

  // This only recharges two full periods.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(250));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());

  // This finishes recharging three full periods.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(250));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
}

TEST_F(AccessRateLimiterTest, IncrementalRechargeToMax) {
  limiter_.reset(new extensions::AccessRateLimiter(
      5, base::TimeDelta::FromMilliseconds(100), &test_clock_));

  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());

  // This only recharges two full periods.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(250));
  // This finishes recharging three full periods, but will not recharge over the
  // additional periods.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(450));
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_TRUE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
  EXPECT_FALSE(limiter_->AttemptAccess());
}
