// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_profile_attributes_updater_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEmail[] = "example@email.com";

}  // namespace

class SigninProfileAttributesUpdaterTest : public testing::Test {
 public:
  SigninProfileAttributesUpdaterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile::TestingFactories testing_factories(
#if defined(OS_CHROMEOS)
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactoriesWithPrimaryAccountSet(kEmail));
#else
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
#endif
    testing_factories.emplace_back(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    std::string name = "profile_name";
    profile_ = profile_manager_.CreateTestingProfile(
        name, /*prefs=*/nullptr, base::UTF8ToUTF16(name), 0, std::string(),
        std::move(testing_factories));
    ASSERT_TRUE(profile_);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_ = nullptr;  // Owned by the profile manager.
};

#if !defined(OS_CHROMEOS)
// Tests that the browser state info is updated on signin and signout.
// ChromeOS does not support signout.
TEST_F(SigninProfileAttributesUpdaterTest, SigninSignout) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));
  ASSERT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsSigninRequired());

  // Signin.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  identity::MakePrimaryAccountAvailable(identity_manager, kEmail);
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(identity::GetTestGaiaIdForEmail(kEmail), entry->GetGAIAId());
  EXPECT_EQ(kEmail, base::UTF16ToUTF8(entry->GetUserName()));

  // Signout.
  identity::ClearPrimaryAccount(identity_manager);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsSigninRequired());
}
#endif  // !defined(OS_CHROMEOS)

// Tests that the browser state info is updated on auth error change.
TEST_F(SigninProfileAttributesUpdaterTest, AuthError) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
#if !defined(OS_CHROMEOS)
  // ChromeOS is signed in at creation.
  identity::MakePrimaryAccountAvailable(identity_manager, kEmail);
#endif

  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsAuthError());

  // Set auth error.
  std::string account_id = identity_manager->GetPrimaryAccountId();
  identity::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(entry->IsAuthError());

  // Remove auth error.
  identity::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(entry->IsAuthError());
}

#if !defined(OS_CHROMEOS)
class SigninProfileAttributesUpdaterWithForceSigninTest
    : public SigninProfileAttributesUpdaterTest {
  void SetUp() override {
    signin_util::SetForceSigninForTesting(true);
    SigninProfileAttributesUpdaterTest::SetUp();
  }

  void TearDown() override {
    SigninProfileAttributesUpdaterTest::TearDown();
    signin_util::ResetForceSigninForTesting();
  }
};

TEST_F(SigninProfileAttributesUpdaterWithForceSigninTest, IsSigninRequired) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_TRUE(entry->IsSigninRequired());

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  AccountInfo account_info =
      identity::MakePrimaryAccountAvailable(identity_manager, kEmail);

  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(identity::GetTestGaiaIdForEmail(kEmail), entry->GetGAIAId());
  EXPECT_EQ(kEmail, base::UTF16ToUTF8(entry->GetUserName()));

  identity::ClearPrimaryAccount(identity_manager);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_TRUE(entry->IsSigninRequired());
}
#endif
