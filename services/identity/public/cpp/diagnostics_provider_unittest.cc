// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/diagnostics_provider_impl.h"

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kAccountId[] = "user@gmail.com";

namespace {

class DiagnosticsProviderTest : public testing::Test {
 public:
  DiagnosticsProviderTest() = default;

  identity::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  identity::DiagnosticsProvider* diagnostics_provider() {
    return identity_test_env_.identity_manager()->GetDiagnosticsProvider();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  identity::IdentityTestEnvironment identity_test_env_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsProviderTest);
};

}  // namespace

TEST_F(DiagnosticsProviderTest, Basic) {
  // Accessing the DiagnosticProvider should not crash.
  ASSERT_TRUE(identity_test_env()->identity_manager());
  EXPECT_TRUE(
      identity_test_env()->identity_manager()->GetDiagnosticsProvider());
}

TEST_F(DiagnosticsProviderTest, GetDetailedStateOfLoadingOfRefreshTokens) {
  EXPECT_EQ(OAuth2TokenServiceDelegate::LoadCredentialsState::
                LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            diagnostics_provider()->GetDetailedStateOfLoadingOfRefreshTokens());
}

TEST_F(DiagnosticsProviderTest, GetDelayBeforeMakingAccessTokenRequests) {
  base::TimeDelta zero;
  EXPECT_EQ(diagnostics_provider()->GetDelayBeforeMakingAccessTokenRequests(),
            zero);
  std::string account_id =
      identity_test_env()->MakeAccountAvailable(kAccountId).account_id;
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_id, GoogleServiceAuthError(
                      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE));
  EXPECT_GT(diagnostics_provider()->GetDelayBeforeMakingAccessTokenRequests(),
            zero);
}

TEST_F(DiagnosticsProviderTest, GetDelayBeforeMakingCookieRequests) {
  base::TimeDelta zero;
  identity_test_env()
      ->identity_manager()
      ->GetAccountsCookieMutator()
      ->AddAccountToCookie(kAccountId, gaia::GaiaSource::kChrome,
                           base::DoNothing());
  EXPECT_EQ(diagnostics_provider()->GetDelayBeforeMakingCookieRequests(), zero);

  identity_test_env()->SimulateMergeSessionFailure(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_GT(diagnostics_provider()->GetDelayBeforeMakingCookieRequests(), zero);
}