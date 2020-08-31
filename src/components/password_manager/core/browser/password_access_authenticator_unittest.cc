// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_access_authenticator.h"

#include <utility>

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::Values;

namespace password_manager {

using metrics_util::ReauthResult;

using MockReauthCallback =
    base::MockCallback<PasswordAccessAuthenticator::ReauthCallback>;

constexpr char kHistogramName[] =
    "PasswordManager.ReauthToAccessPasswordInSettings";

class PasswordAccessAuthenticatorTest : public TestWithParam<ReauthPurpose> {
 public:
  PasswordAccessAuthenticatorTest() {
    ON_CALL(callback_, Run).WillByDefault(Return(true));
  }

  ReauthPurpose purpose() { return GetParam(); }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockReauthCallback& callback() { return callback_; }
  PasswordAccessAuthenticator& authenticator() { return authenticator_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  MockReauthCallback callback_;
  PasswordAccessAuthenticator authenticator_{callback_.Get()};
};

// Check that a passed authentication does not expire before kAuthValidityPeriod
// and does expire after kAuthValidityPeriod.
TEST_P(PasswordAccessAuthenticatorTest, Expiration) {
  EXPECT_CALL(callback(), Run(purpose()));
  EXPECT_TRUE(authenticator().EnsureUserIsAuthenticated(purpose()));
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       1);

  task_environment().AdvanceClock(
      PasswordAccessAuthenticator::kAuthValidityPeriod -
      base::TimeDelta::FromSeconds(1));
  EXPECT_CALL(callback(), Run).Times(0);
  EXPECT_TRUE(authenticator().EnsureUserIsAuthenticated(purpose()));
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSkipped,
                                       1);

  task_environment().AdvanceClock(base::TimeDelta::FromSeconds(2));
  EXPECT_CALL(callback(), Run(purpose()));
  EXPECT_TRUE(authenticator().EnsureUserIsAuthenticated(purpose()));
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       2);
}

// Check that a forced authentication ignores previous successful challenges.
TEST_P(PasswordAccessAuthenticatorTest, ForceReauth) {
  EXPECT_CALL(callback(), Run(purpose()));
  EXPECT_TRUE(authenticator().EnsureUserIsAuthenticated(purpose()));
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       1);

  EXPECT_CALL(callback(), Run(purpose()));
  EXPECT_TRUE(authenticator().ForceUserReauthentication(purpose()));
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       2);
}

// Check that a failed authentication does not start the grace period for
// skipping authentication.
TEST_P(PasswordAccessAuthenticatorTest, Failed) {
  EXPECT_CALL(callback(), Run(purpose())).WillOnce(Return(false));
  EXPECT_FALSE(authenticator().EnsureUserIsAuthenticated(purpose()));
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kFailure,
                                       1);

  // Advance just a little bit, so that if |authenticator| starts the grace
  // period, this is still within it.
  task_environment().AdvanceClock(base::TimeDelta::FromSeconds(1));
  EXPECT_CALL(callback(), Run(purpose())).WillOnce(Return(false));
  EXPECT_FALSE(authenticator().EnsureUserIsAuthenticated(purpose()));
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kFailure,
                                       2);
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordAccessAuthenticatorTest,
                         Values(ReauthPurpose::VIEW_PASSWORD,
                                ReauthPurpose::COPY_PASSWORD,
                                ReauthPurpose::EDIT_PASSWORD,
                                ReauthPurpose::EXPORT));

}  // namespace password_manager
