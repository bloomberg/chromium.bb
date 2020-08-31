// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"

#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

const char kWindowKey[] = "window_key";
const char kGaiaId[] = "fake_gaia_id";
const char kConsentResult[] = "CAESCUVOQ1JZUFRFRBoMZmFrZV9nYWlhX2lk";

class FakeWebAuthFlowWithWindowKey : public WebAuthFlow {
 public:
  explicit FakeWebAuthFlowWithWindowKey(WebAuthFlow::Delegate* delegate,
                                        std::string window_key)
      : WebAuthFlow(delegate,
                    nullptr,
                    GURL(),
                    WebAuthFlow::INTERACTIVE,
                    WebAuthFlow::GET_AUTH_TOKEN),
        fake_window_key_(window_key) {}

  ~FakeWebAuthFlowWithWindowKey() override = default;

  void Start() override {}

  const std::string& GetAppWindowKey() const override {
    return fake_window_key_;
  }

 private:
  const std::string fake_window_key_;
};

class TestGaiaRemoteConsentFlow : public GaiaRemoteConsentFlow {
 public:
  TestGaiaRemoteConsentFlow(GaiaRemoteConsentFlow::Delegate* delegate,
                            const ExtensionTokenKey& token_key,
                            const RemoteConsentResolutionData& resolution_data,
                            const std::string& window_key)
      : GaiaRemoteConsentFlow(delegate, nullptr, token_key, resolution_data),
        window_key_(window_key) {
    SetWebAuthFlowForTesting(
        std::make_unique<FakeWebAuthFlowWithWindowKey>(this, window_key_));
  }

 private:
  const std::string window_key_;
};

class MockGaiaRemoteConsentFlowDelegate
    : public GaiaRemoteConsentFlow::Delegate {
 public:
  MOCK_METHOD1(OnGaiaRemoteConsentFlowFailed,
               void(GaiaRemoteConsentFlow::Failure failure));
  MOCK_METHOD2(OnGaiaRemoteConsentFlowApproved,
               void(const std::string& consent_result,
                    const std::string& gaia_id));
};

class IdentityGaiaRemoteConsentFlowTest : public testing::Test {
 public:
  IdentityGaiaRemoteConsentFlowTest() = default;

  void TearDown() override {
    testing::Test::TearDown();
    base::RunLoop()
        .RunUntilIdle();  // Run tasks so all FakeWebAuthFlowWithWindowKey get
                          // deleted.
  }

  std::unique_ptr<TestGaiaRemoteConsentFlow> CreateTestFlow(
      const std::string& window_key) {
    return CreateTestFlow(window_key, &delegate_);
  }

  std::unique_ptr<TestGaiaRemoteConsentFlow> CreateTestFlow(
      const std::string& window_key,
      GaiaRemoteConsentFlow::Delegate* delegate) {
    ExtensionTokenKey token_key("extension_id", CoreAccountId("account_id"),
                                std::set<std::string>());
    RemoteConsentResolutionData resolution_data;
    resolution_data.url = GURL("https://example.com/auth/");
    return std::unique_ptr<TestGaiaRemoteConsentFlow>(
        new TestGaiaRemoteConsentFlow(delegate, token_key, resolution_data,
                                      window_key));
  }

 protected:
  base::test::TaskEnvironment task_env_;
  testing::StrictMock<MockGaiaRemoteConsentFlowDelegate> delegate_;
};

TEST_F(IdentityGaiaRemoteConsentFlowTest, ConsentResult) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow(kWindowKey);
  EXPECT_CALL(delegate_,
              OnGaiaRemoteConsentFlowApproved(kConsentResult, kGaiaId));
  flow->OnConsentResultSet(kConsentResult, kWindowKey);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, ConsentResult_WrongWindowIgnored) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow(kWindowKey);
  // No call is expected.
  flow->OnConsentResultSet(kConsentResult, "another_window_key");
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, ConsentResult_TwoWindows) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow(kWindowKey);
  const char kWindowKey2[] = "window_key2";
  testing::StrictMock<MockGaiaRemoteConsentFlowDelegate> delegate2;
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow2 =
      CreateTestFlow(kWindowKey2, &delegate2);

  const char kConsentResult2[] = "CAESCkVOQ1JZUFRFRDI";
  EXPECT_CALL(delegate2, OnGaiaRemoteConsentFlowApproved(kConsentResult2, ""));
  flow2->OnConsentResultSet(kConsentResult2, kWindowKey2);

  EXPECT_CALL(delegate_,
              OnGaiaRemoteConsentFlowApproved(kConsentResult, kGaiaId));
  flow->OnConsentResultSet(kConsentResult, kWindowKey);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, InvalidConsentResult) {
  const char kInvalidConsentResult[] = "abc";
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow(kWindowKey);
  EXPECT_CALL(delegate_,
              OnGaiaRemoteConsentFlowFailed(
                  GaiaRemoteConsentFlow::Failure::INVALID_CONSENT_RESULT));
  flow->OnConsentResultSet(kInvalidConsentResult, kWindowKey);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, NoGrant) {
  const char kNoGrantConsentResult[] = "CAA";
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow(kWindowKey);
  EXPECT_CALL(delegate_, OnGaiaRemoteConsentFlowFailed(
                             GaiaRemoteConsentFlow::Failure::NO_GRANT));
  flow->OnConsentResultSet(kNoGrantConsentResult, kWindowKey);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, SetAccountsFailure) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow(kWindowKey);
  EXPECT_CALL(
      delegate_,
      OnGaiaRemoteConsentFlowFailed(
          GaiaRemoteConsentFlow::Failure::SET_ACCOUNTS_IN_COOKIE_FAILED));
  flow->OnSetAccountsComplete(
      signin::SetAccountsInCookieResult::kPersistentError);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, WebAuthFlowFailure_WindowClosed) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow(kWindowKey);
  EXPECT_CALL(delegate_, OnGaiaRemoteConsentFlowFailed(
                             GaiaRemoteConsentFlow::Failure::WINDOW_CLOSED));
  flow->OnAuthFlowFailure(WebAuthFlow::Failure::WINDOW_CLOSED);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, WebAuthFlowFailure_LoadFailed) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow(kWindowKey);
  EXPECT_CALL(delegate_, OnGaiaRemoteConsentFlowFailed(
                             GaiaRemoteConsentFlow::Failure::LOAD_FAILED));
  flow->OnAuthFlowFailure(WebAuthFlow::Failure::LOAD_FAILED);
}

}  // namespace extensions
