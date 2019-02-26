// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_downloader.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kTestEmail = "test@example.com";
const std::string kTestGaia = "gaia";
const std::string kTestHostedDomain = "google.com";
const std::string kTestFullName = "full_name";
const std::string kTestGivenName = "given_name";
const std::string kTestLocale = "locale";
const std::string kTestValidPictureURL = "http://www.google.com/";
const std::string kTestInvalidPictureURL = "invalid_picture_url";

} // namespace

class ProfileDownloaderTest
    : public testing::Test,
      public ProfileDownloaderDelegate,
      public identity::IdentityManager::DiagnosticsObserver {
 protected:
  ProfileDownloaderTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ProfileDownloaderTest() override {}

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        AccountFetcherServiceFactory::GetInstance(),
        base::BindRepeating(&FakeAccountFetcherServiceBuilder::BuildForTests));

    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);

    account_tracker_service_ =
        AccountTrackerServiceFactory::GetForProfile(profile_.get());
    account_fetcher_service_ = static_cast<FakeAccountFetcherService*>(
        AccountFetcherServiceFactory::GetForProfile(profile_.get()));
    profile_downloader_.reset(new ProfileDownloader(this));

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_test_env_ =
        identity_test_env_profile_adaptor_->identity_test_env();
    DCHECK(identity_test_env_);

    identity_test_env_->identity_manager()->AddDiagnosticsObserver(this);
  }

  void TearDown() override {
    identity_test_env_->identity_manager()->RemoveDiagnosticsObserver(this);
  }

  bool NeedsProfilePicture() const override { return true; };
  int GetDesiredImageSideLength() const override { return 128; };
  std::string GetCachedPictureURL() const override { return ""; };
  Profile* GetBrowserProfile() override { return profile_.get(); };
  bool IsPreSignin() const override { return false; }
  void OnProfileDownloadSuccess(ProfileDownloader* downloader) override {

  }
  void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) override {}

  void SimulateUserInfoSuccess(const std::string& picture_url) {
    account_fetcher_service_->FakeUserInfoFetchSuccess(
        account_tracker_service_->PickAccountIdForAccount(kTestGaia,
                                                          kTestEmail),
        kTestEmail, kTestGaia, kTestHostedDomain, kTestFullName, kTestGivenName,
        kTestLocale, picture_url);
  }

  // IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(const std::string& account_id,
                              const std::string& consumer_id,
                              const identity::ScopeSet& scopes) override {
    // This flow should be invoked only when a test has explicitly set up
    // preconditions so that ProfileDownloader will request access tokens.
    DCHECK(!on_access_token_request_callback_.is_null());

    account_id_for_access_token_request_ = account_id;

    std::move(on_access_token_request_callback_).Run();
  }

  void set_on_access_token_requested_callback(base::OnceClosure callback) {
    on_access_token_request_callback_ = std::move(callback);
  }

  AccountTrackerService* account_tracker_service_;
  FakeAccountFetcherService* account_fetcher_service_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  identity::IdentityTestEnvironment* identity_test_env_;
  base::OnceClosure on_access_token_request_callback_;
  std::string account_id_for_access_token_request_;
  std::unique_ptr<ProfileDownloader> profile_downloader_;
};

TEST_F(ProfileDownloaderTest, FetchAccessToken) {
  std::string account_id =
      account_tracker_service_->SeedAccountInfo(kTestGaia, kTestEmail);
  identity_test_env_->SetRefreshTokenForAccount(account_id);

  base::RunLoop run_loop;
  set_on_access_token_requested_callback(run_loop.QuitClosure());
  profile_downloader_->StartForAccount(account_id);
  run_loop.Run();

  EXPECT_EQ(account_id, account_id_for_access_token_request_);
}

TEST_F(ProfileDownloaderTest, AccountInfoReady) {
  std::string account_id =
      account_tracker_service_->SeedAccountInfo(kTestGaia, kTestEmail);
  SimulateUserInfoSuccess(kTestValidPictureURL);

  ASSERT_EQ(ProfileDownloader::PICTURE_FAILED,
            profile_downloader_->GetProfilePictureStatus());
  profile_downloader_->StartForAccount(account_id);
  profile_downloader_->StartFetchingImage();
  ASSERT_EQ(kTestValidPictureURL, profile_downloader_->GetProfilePictureURL());
}

TEST_F(ProfileDownloaderTest, AccountInfoNotReady) {
  std::string account_id =
      account_tracker_service_->SeedAccountInfo(kTestGaia, kTestEmail);

  ASSERT_EQ(ProfileDownloader::PICTURE_FAILED,
            profile_downloader_->GetProfilePictureStatus());
  profile_downloader_->StartForAccount(account_id);
  profile_downloader_->StartFetchingImage();
  SimulateUserInfoSuccess(kTestValidPictureURL);
  ASSERT_EQ(kTestValidPictureURL, profile_downloader_->GetProfilePictureURL());
}

// Regression test for http://crbug.com/854907
TEST_F(ProfileDownloaderTest, AccountInfoNoPictureDoesNotCrash) {
  std::string account_id =
      account_tracker_service_->SeedAccountInfo(kTestGaia, kTestEmail);
  SimulateUserInfoSuccess(AccountTrackerService::kNoPictureURLFound);

  profile_downloader_->StartForAccount(account_id);
  profile_downloader_->StartFetchingImage();

  EXPECT_TRUE(profile_downloader_->GetProfilePictureURL().empty());
  ASSERT_EQ(ProfileDownloader::PICTURE_DEFAULT,
            profile_downloader_->GetProfilePictureStatus());
}

// Regression test for http://crbug.com/854907
TEST_F(ProfileDownloaderTest, AccountInfoInvalidPictureURLDoesNotCrash) {
  std::string account_id =
      account_tracker_service_->SeedAccountInfo(kTestGaia, kTestEmail);
  SimulateUserInfoSuccess(kTestInvalidPictureURL);

  profile_downloader_->StartForAccount(account_id);
  profile_downloader_->StartFetchingImage();

  EXPECT_TRUE(profile_downloader_->GetProfilePictureURL().empty());
  ASSERT_EQ(ProfileDownloader::PICTURE_FAILED,
            profile_downloader_->GetProfilePictureStatus());
}
