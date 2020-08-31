// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/image_fetcher/core/image_data_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "components/signin/internal/identity_manager/child_account_info_fetcher_android.h"
#endif

namespace {
// Simple wrapper around a static string; used to avoid implicit conversion
// of the account key to an std::string (which is the type used for account
// identifier). This is a POD type so it can be used for static storage const
// variables. It must not implicitly convert to std::string.
struct AccountKey {
  const char* value;
};

const AccountKey kAccountKeyAlpha = {"alpha"};
const AccountKey kAccountKeyBeta = {"beta"};
const AccountKey kAccountKeyGamma = {"gamma"};
const AccountKey kAccountKeyChild = {"child"};
const AccountKey kAccountKeyIncomplete = {"incomplete"};
const AccountKey kAccountKeyFooBar = {"foobar"};
const AccountKey kAccountKeyFooDotBar = {"foo.bar"};

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS) && !defined(OS_IOS)
const AccountKey kAccountKeyAdvancedProtection = {"advanced_protection"};
#endif

const char kTokenInfoResponseFormat[] =
    "{                        \
      \"id\": \"%s\",         \
      \"email\": \"%s\",      \
      \"hd\": \"\",           \
      \"name\": \"%s\",       \
      \"given_name\": \"%s\", \
      \"locale\": \"%s\",     \
      \"picture\": \"%s\"     \
    }";

const char kTokenInfoIncompleteResponseFormat[] =
    "{                        \
      \"id\": \"%s\",         \
      \"email\": \"%s\",      \
      \"hd\": \"\",           \
    }";

enum TrackingEventType {
  UPDATED,
  REMOVED,
};

std::string AccountKeyToEmail(AccountKey account_key) {
  return base::StringPrintf("%s@gmail.com", account_key.value);
}

std::string AccountKeyToGaiaId(AccountKey account_key) {
  return base::StringPrintf("gaia-%s", account_key.value);
}

std::string AccountKeyToFullName(AccountKey account_key) {
  return base::StringPrintf("full-name-%s", account_key.value);
}

std::string AccountKeyToGivenName(AccountKey account_key) {
  return base::StringPrintf("given-name-%s", account_key.value);
}

std::string AccountKeyToLocale(AccountKey account_key) {
  return base::StringPrintf("locale-%s", account_key.value);
}

std::string AccountKeyToPictureURL(AccountKey account_key) {
  return base::StringPrintf(
      "https://example.com/-%s"
      "/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg",
      account_key.value);
}

std::string AccountKeyToPictureURLWithSize(AccountKey account_key) {
  return signin::GetAvatarImageURLWithOptions(
             GURL(AccountKeyToPictureURL(account_key)),
             signin::kAccountInfoImageSize, true /* no_silhouette */)
      .spec();
}

class TrackingEvent {
 public:
  TrackingEvent(TrackingEventType type,
                const CoreAccountId& account_id,
                const std::string& gaia_id,
                const std::string& email)
      : type_(type),
        account_id_(account_id),
        gaia_id_(gaia_id),
        email_(email) {}

  bool operator==(const TrackingEvent& event) const {
    return type_ == event.type_ && account_id_ == event.account_id_ &&
           gaia_id_ == event.gaia_id_ && email_ == event.email_;
  }

  std::string ToString() const {
    const char* typestr = "INVALID";
    switch (type_) {
      case UPDATED:
        typestr = "UPD";
        break;
      case REMOVED:
        typestr = "REM";
        break;
    }
    return base::StringPrintf(
        "{ type: %s, account_id: %s, gaia: %s, email: %s }", typestr,
        account_id_.ToString().c_str(), gaia_id_.c_str(), email_.c_str());
  }

  TrackingEventType type_;
  CoreAccountId account_id_;
  std::string gaia_id_;
  std::string email_;
};

std::string Str(const std::vector<TrackingEvent>& events) {
  std::string str = "[";
  bool needs_comma = false;
  for (const TrackingEvent& event : events) {
    if (needs_comma)
      str += ",\n ";
    needs_comma = true;
    str += event.ToString();
  }
  str += "]";
  return str;
}

}  // namespace

class AccountTrackerServiceTest : public testing::Test {
 public:
  AccountTrackerServiceTest()
      : signin_client_(&pref_service_),
        fake_oauth2_token_service_(&pref_service_) {
#if defined(OS_ANDROID)
    ChildAccountInfoFetcherAndroid::InitializeForTests();
#endif

    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    AccountFetcherService::RegisterPrefs(pref_service_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
  }

  ~AccountTrackerServiceTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    CreateAccountTracker(base::FilePath(), /*network_enabled=*/true);
    fake_oauth2_token_service_.LoadCredentials(CoreAccountId());
  }

  void TearDown() override {
    DeleteAccountTracker();
    testing::Test::TearDown();
  }

  void ResetAccountTracker() {
    DeleteAccountTracker();
    CreateAccountTracker(base::FilePath(), /*network_enabled=*/true);
  }

  void ResetAccountTrackerNetworkDisabled() {
    DeleteAccountTracker();
    CreateAccountTracker(base::FilePath(), /*network_enabled=*/false);
  }

  void ResetAccountTrackerWithPersistence(base::FilePath path) {
    DeleteAccountTracker();
    CreateAccountTracker(std::move(path), /*network_enabled=*/true);
  }

  void SimulateTokenAvailable(AccountKey account_key) {
    fake_oauth2_token_service_.UpdateCredentials(
        AccountKeyToAccountId(account_key), "fake-refresh-token");
  }

  void SimulateTokenRevoked(AccountKey account_key) {
    fake_oauth2_token_service_.RevokeCredentials(
        AccountKeyToAccountId(account_key));
  }

  // Helpers to fake access token and user info fetching
  CoreAccountId AccountKeyToAccountId(AccountKey account_key) {
    if (force_account_id_to_email_for_legacy_tests_)
      return CoreAccountId(AccountKeyToEmail(account_key));

    return AccountTrackerService::PickAccountIdForAccount(
        &pref_service_, AccountKeyToGaiaId(account_key),
        AccountKeyToEmail(account_key));
  }

  void CheckAccountDetails(AccountKey account_key, const AccountInfo& info) {
    EXPECT_EQ(AccountKeyToAccountId(account_key), info.account_id);
    EXPECT_EQ(AccountKeyToGaiaId(account_key), info.gaia);
    EXPECT_EQ(AccountKeyToEmail(account_key), info.email);
    EXPECT_EQ(kNoHostedDomainFound, info.hosted_domain);
    EXPECT_EQ(AccountKeyToFullName(account_key), info.full_name);
    EXPECT_EQ(AccountKeyToGivenName(account_key), info.given_name);
    EXPECT_EQ(AccountKeyToLocale(account_key), info.locale);
  }

  testing::AssertionResult CheckAccountTrackerEvents(
      const std::vector<TrackingEvent>& events) {
    std::string maybe_newline;
    if ((events.size() + account_tracker_events_.size()) > 2)
      maybe_newline = "\n";

    testing::AssertionResult result(
        (account_tracker_events_ == events)
            ? testing::AssertionSuccess()
            : (testing::AssertionFailure()
               << "Expected " << maybe_newline << Str(events) << ", "
               << maybe_newline << "Got " << maybe_newline
               << Str(account_tracker_events_)));

    account_tracker_events_.clear();
    return result;
  }

  void ClearAccountTrackerEvents() { account_tracker_events_.clear(); }

  void OnAccountUpdated(const AccountInfo& ids) {
    account_tracker_events_.push_back(
        TrackingEvent(UPDATED, ids.account_id, ids.gaia, ids.email));
  }

  void OnAccountRemoved(const AccountInfo& ids) {
    account_tracker_events_.push_back(
        TrackingEvent(REMOVED, ids.account_id, ids.gaia, ids.email));
  }

  // Helpers to fake access token and user info fetching
  void IssueAccessToken(AccountKey account_key) {
    fake_oauth2_token_service_.IssueAllTokensForAccount(
        AccountKeyToAccountId(account_key),
        OAuth2AccessTokenConsumer::TokenResponse(
            base::StringPrintf("access_token-%s", account_key.value),
            base::Time::Max(), std::string()));
  }

  std::string GenerateValidTokenInfoResponse(AccountKey account_key) {
    return base::StringPrintf(kTokenInfoResponseFormat,
                              AccountKeyToGaiaId(account_key).c_str(),
                              AccountKeyToEmail(account_key).c_str(),
                              AccountKeyToFullName(account_key).c_str(),
                              AccountKeyToGivenName(account_key).c_str(),
                              AccountKeyToLocale(account_key).c_str(),
                              AccountKeyToPictureURL(account_key).c_str());
  }

  std::string GenerateIncompleteTokenInfoResponse(AccountKey account_key) {
    return base::StringPrintf(kTokenInfoIncompleteResponseFormat,
                              AccountKeyToGaiaId(account_key).c_str(),
                              AccountKeyToEmail(account_key).c_str());
  }
  void ReturnAccountInfoFetchSuccess(AccountKey account_key);
  void ReturnAccountInfoFetchSuccessIncomplete(AccountKey account_key);
  void ReturnAccountInfoFetchFailure(AccountKey account_key);
  void ReturnAccountImageFetchSuccess(AccountKey account_key);
  void ReturnAccountImageFetchFailure(AccountKey account_key);

  AccountFetcherService* account_fetcher() { return account_fetcher_.get(); }
  AccountTrackerService* account_tracker() { return account_tracker_.get(); }
  FakeProfileOAuth2TokenService* token_service() {
    return &fake_oauth2_token_service_;
  }
  SigninClient* signin_client() { return &signin_client_; }
  PrefService* prefs() { return &pref_service_; }

  network::TestURLLoaderFactory* GetTestURLLoaderFactory() {
    return signin_client_.GetTestURLLoaderFactory();
  }

  bool* force_account_id_to_email_for_legacy_tests_pointer() {
    return &force_account_id_to_email_for_legacy_tests_;
  }

 protected:
  void ReturnFetchResults(net::HttpStatusCode response_code,
                          const std::string& response_string);

  base::test::TaskEnvironment task_environment_;

 private:
  void CreateAccountTracker(base::FilePath path, bool network_enabled) {
    DCHECK(!account_tracker_);
    DCHECK(!account_fetcher_);

    pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);

    account_tracker_ = std::make_unique<AccountTrackerService>();
    account_fetcher_ = std::make_unique<AccountFetcherService>();

    // Register callbacks before initialisation to allow the tests to check the
    // events that are triggered during the initialisation.
    account_tracker_->SetOnAccountUpdatedCallback(base::BindRepeating(
        &AccountTrackerServiceTest::OnAccountUpdated, base::Unretained(this)));
    account_tracker_->SetOnAccountRemovedCallback(base::BindRepeating(
        &AccountTrackerServiceTest::OnAccountRemoved, base::Unretained(this)));

    account_tracker_->Initialize(&pref_service_, std::move(path));
    account_fetcher_->Initialize(
        signin_client(), token_service(), account_tracker_.get(),
        std::make_unique<image_fetcher::FakeImageDecoder>());
    if (network_enabled) {
      account_fetcher_->EnableNetworkFetchesForTest();
    }
  }

  void DeleteAccountTracker() {
    if (account_fetcher_) {
      account_fetcher_->Shutdown();
      account_fetcher_.reset();
    }

    if (account_tracker_) {
      account_tracker_->Shutdown();
      account_tracker_.reset();
    }
    // Allow residual |account_tracker_| posted tasks to run.
    task_environment_.RunUntilIdle();
  }

  TestingPrefServiceSimple pref_service_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService fake_oauth2_token_service_;
  std::unique_ptr<AccountFetcherService> account_fetcher_;
  std::unique_ptr<AccountTrackerService> account_tracker_;
  std::vector<TrackingEvent> account_tracker_events_;
  bool force_account_id_to_email_for_legacy_tests_ = false;
};

void AccountTrackerServiceTest::ReturnFetchResults(
    net::HttpStatusCode response_code,
    const std::string& response_string) {
  GURL url = GaiaUrls::GetInstance()->oauth_user_info_url();
  EXPECT_TRUE(GetTestURLLoaderFactory()->IsPending(url.spec()));

  // It's possible for multiple requests to be pending. Respond to all of them.
  while (GetTestURLLoaderFactory()->IsPending(url.spec())) {
    GetTestURLLoaderFactory()->SimulateResponseForPendingRequest(
        url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(response_code), response_string,
        network::TestURLLoaderFactory::kMostRecentMatch);
  }
}

void AccountTrackerServiceTest::ReturnAccountInfoFetchSuccess(
    AccountKey account_key) {
  IssueAccessToken(account_key);
  ReturnFetchResults(net::HTTP_OK, GenerateValidTokenInfoResponse(account_key));
}

void AccountTrackerServiceTest::ReturnAccountInfoFetchSuccessIncomplete(
    AccountKey account_key) {
  IssueAccessToken(account_key);
  ReturnFetchResults(net::HTTP_OK,
                     GenerateIncompleteTokenInfoResponse(account_key));
}

void AccountTrackerServiceTest::ReturnAccountInfoFetchFailure(
    AccountKey account_key) {
  IssueAccessToken(account_key);
  ReturnFetchResults(net::HTTP_BAD_REQUEST, std::string());
}

void AccountTrackerServiceTest::ReturnAccountImageFetchSuccess(
    AccountKey account_key) {
  GetTestURLLoaderFactory()->AddResponse(
      AccountKeyToPictureURLWithSize(account_key), "image data");
  task_environment_.RunUntilIdle();
}

void AccountTrackerServiceTest::ReturnAccountImageFetchFailure(
    AccountKey account_key) {
  GetTestURLLoaderFactory()->AddResponse(
      AccountKeyToPictureURLWithSize(account_key), std::string(),
      net::HTTP_BAD_REQUEST);
  task_environment_.RunUntilIdle();
}

TEST_F(AccountTrackerServiceTest, Basic) {}

TEST_F(AccountTrackerServiceTest, TokenAvailable) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  EXPECT_FALSE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({}));
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_Revoked) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  SimulateTokenRevoked(kAccountKeyAlpha);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({}));
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_UserInfo_ImageSuccess) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));

  AccountInfo account_info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyAlpha));
  EXPECT_TRUE(account_info.account_image.IsEmpty());
  EXPECT_TRUE(account_info.last_downloaded_image_url_with_size.empty());
  ReturnAccountImageFetchSuccess(kAccountKeyAlpha);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));
  account_info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyAlpha));
  EXPECT_FALSE(account_info.account_image.IsEmpty());
  EXPECT_EQ(account_info.last_downloaded_image_url_with_size,
            AccountKeyToPictureURLWithSize(kAccountKeyAlpha));
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_UserInfo_ImageFailure) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));

  AccountInfo account_info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyAlpha));
  EXPECT_TRUE(account_info.account_image.IsEmpty());
  EXPECT_TRUE(account_info.last_downloaded_image_url_with_size.empty());
  ReturnAccountImageFetchFailure(kAccountKeyAlpha);
  account_info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyAlpha));
  EXPECT_TRUE(account_info.account_image.IsEmpty());
  EXPECT_TRUE(account_info.last_downloaded_image_url_with_size.empty());
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_UserInfo_Revoked) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));
  SimulateTokenRevoked(kAccountKeyAlpha);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(REMOVED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_UserInfoFailed) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchFailure(kAccountKeyAlpha);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({}));
}

TEST_F(AccountTrackerServiceTest, TokenAvailableTwice_UserInfoOnce) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));

  SimulateTokenAvailable(kAccountKeyAlpha);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({}));
}

TEST_F(AccountTrackerServiceTest, TokenAlreadyExists) {
  SimulateTokenAvailable(kAccountKeyAlpha);

  EXPECT_FALSE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({}));
}

TEST_F(AccountTrackerServiceTest, TwoTokenAvailable_TwoUserInfo) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  SimulateTokenAvailable(kAccountKeyBeta);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyBeta);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyBeta),
                    AccountKeyToGaiaId(kAccountKeyBeta),
                    AccountKeyToEmail(kAccountKeyBeta)),
  }));
}

TEST_F(AccountTrackerServiceTest, TwoTokenAvailable_OneUserInfo) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  SimulateTokenAvailable(kAccountKeyBeta);
  ReturnAccountInfoFetchSuccess(kAccountKeyBeta);
  EXPECT_FALSE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyBeta),
                    AccountKeyToGaiaId(kAccountKeyBeta),
                    AccountKeyToEmail(kAccountKeyBeta)),
  }));
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));
}

TEST_F(AccountTrackerServiceTest, GetAccounts) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  SimulateTokenAvailable(kAccountKeyBeta);
  SimulateTokenAvailable(kAccountKeyGamma);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyBeta);
  ReturnAccountInfoFetchSuccess(kAccountKeyGamma);

  std::vector<AccountInfo> infos = account_tracker()->GetAccounts();

  ASSERT_EQ(3u, infos.size());
  CheckAccountDetails(kAccountKeyAlpha, infos[0]);
  CheckAccountDetails(kAccountKeyBeta, infos[1]);
  CheckAccountDetails(kAccountKeyGamma, infos[2]);
}

TEST_F(AccountTrackerServiceTest, GetAccountInfo_Empty) {
  AccountInfo info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyAlpha));
  EXPECT_EQ(CoreAccountId(), info.account_id);
}

TEST_F(AccountTrackerServiceTest, GetAccountInfo_TokenAvailable) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  AccountInfo info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyAlpha));
  EXPECT_EQ(AccountKeyToAccountId(kAccountKeyAlpha), info.account_id);
  EXPECT_EQ(std::string(), info.gaia);
  EXPECT_EQ(std::string(), info.email);
}

TEST_F(AccountTrackerServiceTest, GetAccountInfo_TokenAvailable_UserInfo) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  AccountInfo info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyAlpha));
  CheckAccountDetails(kAccountKeyAlpha, info);
}

TEST_F(AccountTrackerServiceTest, GetAccountInfo_TokenAvailable_EnableNetwork) {
  // Create an account tracker and an account fetcher service but do not
  // enable network fetches.
  ResetAccountTrackerNetworkDisabled();

  SimulateTokenAvailable(kAccountKeyAlpha);
  IssueAccessToken(kAccountKeyAlpha);
  // No fetcher has been created yet.
  EXPECT_EQ(0, GetTestURLLoaderFactory()->NumPending());

  // Enable the network to create the fetcher then issue the access token.
  account_fetcher()->EnableNetworkFetchesForTest();

  // Fetcher was created and executes properly.
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);

  AccountInfo info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyAlpha));
  CheckAccountDetails(kAccountKeyAlpha, info);
}

TEST_F(AccountTrackerServiceTest, FindAccountInfoByGaiaId) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);

  const std::string gaia_id_alpha = AccountKeyToGaiaId(kAccountKeyAlpha);
  AccountInfo info = account_tracker()->FindAccountInfoByGaiaId(gaia_id_alpha);
  EXPECT_EQ(AccountKeyToAccountId(kAccountKeyAlpha), info.account_id);
  EXPECT_EQ(gaia_id_alpha, info.gaia);

  const std::string gaia_id_beta = AccountKeyToGaiaId(kAccountKeyBeta);
  info = account_tracker()->FindAccountInfoByGaiaId(gaia_id_beta);
  EXPECT_TRUE(info.account_id.empty());
}

TEST_F(AccountTrackerServiceTest, FindAccountInfoByEmail) {
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);

  const std::string email_alpha = AccountKeyToEmail(kAccountKeyAlpha);
  AccountInfo info = account_tracker()->FindAccountInfoByEmail(email_alpha);
  EXPECT_EQ(AccountKeyToAccountId(kAccountKeyAlpha), info.account_id);
  EXPECT_EQ(email_alpha, info.email);

  // Should also work with "canonically-equal" email addresses.
  info = account_tracker()->FindAccountInfoByEmail("Alpha@Gmail.COM");
  EXPECT_EQ(AccountKeyToAccountId(kAccountKeyAlpha), info.account_id);
  EXPECT_EQ(email_alpha, info.email);
  info = account_tracker()->FindAccountInfoByEmail("al.pha@gmail.com");
  EXPECT_EQ(AccountKeyToAccountId(kAccountKeyAlpha), info.account_id);
  EXPECT_EQ(email_alpha, info.email);

  const std::string email_beta = AccountKeyToEmail(kAccountKeyBeta);
  info = account_tracker()->FindAccountInfoByEmail(email_beta);
  EXPECT_EQ(CoreAccountId(), info.account_id);
}

TEST_F(AccountTrackerServiceTest, Persistence) {
  // Define a user data directory for the account image storage.
  base::ScopedTempDir scoped_user_data_dir;
  ASSERT_TRUE(scoped_user_data_dir.CreateUniqueTempDir());

  // Create a tracker and add two accounts.  This should cause the accounts
  // to be saved to persistence.
  ResetAccountTrackerWithPersistence(scoped_user_data_dir.GetPath());
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  ReturnAccountImageFetchSuccess(kAccountKeyAlpha);
  SimulateTokenAvailable(kAccountKeyBeta);
  ReturnAccountInfoFetchSuccess(kAccountKeyBeta);
  ReturnAccountImageFetchSuccess(kAccountKeyBeta);

  // Create a new tracker and make sure it loads the accounts (including the
  // images) correctly from persistence.
  ClearAccountTrackerEvents();
  ResetAccountTrackerWithPersistence(scoped_user_data_dir.GetPath());

  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyBeta),
                    AccountKeyToGaiaId(kAccountKeyBeta),
                    AccountKeyToEmail(kAccountKeyBeta)),
  }));
  // Wait until all account images are loaded.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyBeta),
                    AccountKeyToGaiaId(kAccountKeyBeta),
                    AccountKeyToEmail(kAccountKeyBeta)),
  }));

  std::vector<AccountInfo> infos = account_tracker()->GetAccounts();
  ASSERT_EQ(2u, infos.size());
  CheckAccountDetails(kAccountKeyAlpha, infos[0]);
  CheckAccountDetails(kAccountKeyBeta, infos[1]);

  // Remove an account.
  // This will allow testing removal as well as child accounts which is only
  // allowed for a single account.
  SimulateTokenRevoked(kAccountKeyAlpha);
#if defined(OS_ANDROID)
  account_fetcher()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyBeta),
                                       true);
#else
  account_tracker()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyBeta),
                                       true);
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  account_tracker()->SetIsAdvancedProtectionAccount(
      AccountKeyToAccountId(kAccountKeyBeta), true);
#endif

  // Create a new tracker and make sure it loads the single account from
  // persistence. Also verify it is a child account.
  ResetAccountTrackerWithPersistence(scoped_user_data_dir.GetPath());

  infos = account_tracker()->GetAccounts();
  ASSERT_EQ(1u, infos.size());
  CheckAccountDetails(kAccountKeyBeta, infos[0]);
  EXPECT_TRUE(infos[0].is_child_account);
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  EXPECT_TRUE(infos[0].is_under_advanced_protection);
#else
  EXPECT_FALSE(infos[0].is_under_advanced_protection);
#endif

  // Delete the account tracker before cleaning up |scoped_user_data_dir| so
  // that all in-use files are closed.
  ResetAccountTracker();
  ASSERT_TRUE(scoped_user_data_dir.Delete());
}

TEST_F(AccountTrackerServiceTest, SeedAccountInfo) {
  EXPECT_TRUE(account_tracker()->GetAccounts().empty());

  const std::string gaia_id = AccountKeyToGaiaId(kAccountKeyFooBar);
  const std::string email = AccountKeyToEmail(kAccountKeyFooBar);
  const std::string email_dotted = AccountKeyToEmail(kAccountKeyFooDotBar);
  const CoreAccountId account_id =
      account_tracker()->PickAccountIdForAccount(gaia_id, email);

  account_tracker()->SeedAccountInfo(gaia_id, email);
  auto infos = account_tracker()->GetAccounts();
  ASSERT_EQ(1u, infos.size());
  EXPECT_EQ(account_id, infos[0].account_id);
  EXPECT_EQ(gaia_id, infos[0].gaia);
  EXPECT_EQ(email, infos[0].email);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, account_id, gaia_id, email),
  }));

  account_tracker()->SeedAccountInfo(gaia_id, email_dotted);
  infos = account_tracker()->GetAccounts();
  ASSERT_EQ(1u, infos.size()) << "Seeding information to an existing account "
                                 "should not add a new account";
  EXPECT_EQ(account_id, infos[0].account_id)
      << "Account id is either the canonicalized email or gaia, it should "
         "remain the same";
  EXPECT_EQ(gaia_id, infos[0].gaia);
  EXPECT_EQ(email_dotted, infos[0].email) << "Email should be changed";
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, account_id, gaia_id, email_dotted),
  }));
}

TEST_F(AccountTrackerServiceTest, SeedAccountInfoFull) {
  AccountInfo info;
  info.gaia = AccountKeyToGaiaId(kAccountKeyAlpha);
  info.email = AccountKeyToEmail(kAccountKeyAlpha);
  info.full_name = AccountKeyToFullName(kAccountKeyAlpha);
  info.account_id = account_tracker()->SeedAccountInfo(info);

  // Validate that seeding an unexisting account works and sends a
  // notification.
  AccountInfo stored_info = account_tracker()->GetAccountInfo(info.account_id);
  EXPECT_EQ(info.gaia, stored_info.gaia);
  EXPECT_EQ(info.email, stored_info.email);
  EXPECT_EQ(info.full_name, stored_info.full_name);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, info.account_id, info.gaia, info.email),
  }));

  // Validate that seeding new full informations to an existing account works
  // and sends a notification.
  info.given_name = AccountKeyToGivenName(kAccountKeyAlpha);
  info.hosted_domain = kNoHostedDomainFound;
  info.locale = AccountKeyToLocale(kAccountKeyAlpha);
  info.picture_url = AccountKeyToPictureURL(kAccountKeyAlpha);
  account_tracker()->SeedAccountInfo(info);
  stored_info = account_tracker()->GetAccountInfo(info.account_id);
  EXPECT_EQ(info.gaia, stored_info.gaia);
  EXPECT_EQ(info.email, stored_info.email);
  EXPECT_EQ(info.given_name, stored_info.given_name);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, info.account_id, info.gaia, info.email),
  }));

  // Validate that seeding invalid information to an existing account doesn't
  // work and doesn't send a notification.
  info.given_name = std::string();
  account_tracker()->SeedAccountInfo(info);
  stored_info = account_tracker()->GetAccountInfo(info.account_id);
  EXPECT_EQ(info.gaia, stored_info.gaia);
  EXPECT_NE(info.given_name, stored_info.given_name);
  EXPECT_TRUE(CheckAccountTrackerEvents({}));
}

TEST_F(AccountTrackerServiceTest, UpgradeToFullAccountInfo) {
  // Start by simulating an incomplete account info and let it be saved to
  // prefs.
  ResetAccountTracker();
  SimulateTokenAvailable(kAccountKeyIncomplete);
  ReturnAccountInfoFetchSuccessIncomplete(kAccountKeyIncomplete);

  ResetAccountTracker();

  // Validate that the loaded AccountInfo from prefs is considered invalid.
  std::vector<AccountInfo> infos = account_tracker()->GetAccounts();
  ASSERT_EQ(1u, infos.size());
  EXPECT_FALSE(infos[0].IsValid());

  // Simulate the same account getting a refresh token with all the info.
  SimulateTokenAvailable(kAccountKeyIncomplete);
  ReturnAccountInfoFetchSuccess(kAccountKeyIncomplete);

  // Validate that the account is now considered valid.
  infos = account_tracker()->GetAccounts();
  ASSERT_EQ(1u, infos.size());
  EXPECT_TRUE(infos[0].IsValid());

  // Reinstantiate a tracker to validate that the AccountInfo saved to prefs
  // is now the upgraded one, considered valid.
  ClearAccountTrackerEvents();
  ResetAccountTrackerNetworkDisabled();

  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyIncomplete),
                    AccountKeyToGaiaId(kAccountKeyIncomplete),
                    AccountKeyToEmail(kAccountKeyIncomplete)),
  }));

  // Enabling network fetches shouldn't cause any actual fetch since the
  // AccountInfos loaded from prefs should be valid.
  account_fetcher()->EnableNetworkFetchesForTest();

  infos = account_tracker()->GetAccounts();
  ASSERT_EQ(1u, infos.size());
  EXPECT_TRUE(infos[0].IsValid());
  // Check that no network fetches were made.
  EXPECT_TRUE(CheckAccountTrackerEvents({}));
}

TEST_F(AccountTrackerServiceTest, TimerRefresh) {
  // Start by creating a tracker and adding a couple accounts to be persisted
  // to prefs.
  ResetAccountTracker();
  SimulateTokenAvailable(kAccountKeyAlpha);
  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  SimulateTokenAvailable(kAccountKeyBeta);
  ReturnAccountInfoFetchSuccess(kAccountKeyBeta);

  // Rewind the time by half a day, which shouldn't be enough to trigger a
  // network refresh.
  base::Time fake_update = base::Time::Now() - base::TimeDelta::FromHours(12);
  signin_client()->GetPrefs()->SetTime(AccountFetcherService::kLastUpdatePref,
                                       fake_update);

  // Instantiate a new ATS, making sure the persisted accounts are still there
  // and that no network fetches happen.
  ResetAccountTrackerNetworkDisabled();

  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  std::vector<AccountInfo> infos = account_tracker()->GetAccounts();
  ASSERT_EQ(2u, infos.size());
  EXPECT_TRUE(infos[0].IsValid());
  EXPECT_TRUE(infos[1].IsValid());

  account_fetcher()->EnableNetworkFetchesForTest();
  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());

  // Rewind the last updated time enough to trigger a network refresh.
  fake_update = base::Time::Now() - base::TimeDelta::FromHours(25);
  signin_client()->GetPrefs()->SetTime(AccountFetcherService::kLastUpdatePref,
                                       fake_update);

  // Instantiate a new tracker and validate that even though the AccountInfos
  // are still valid, the network fetches are started.
  ResetAccountTrackerNetworkDisabled();

  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  infos = account_tracker()->GetAccounts();
  ASSERT_EQ(2u, infos.size());
  EXPECT_TRUE(infos[0].IsValid());
  EXPECT_TRUE(infos[1].IsValid());

  account_fetcher()->EnableNetworkFetchesForTest();
  EXPECT_FALSE(account_fetcher()->IsAllUserInfoFetched());
}

TEST_F(AccountTrackerServiceTest, LegacyDottedAccountIds) {
  // Force legacy of non-normalized email as account_id.
  base::AutoReset<bool> force_account_id_to_email_for_legacy_test(
      force_account_id_to_email_for_legacy_tests_pointer(), true);

  // Start by creating a tracker and adding an account with a dotted account
  // id because of an old bug in token service.  The token service would also
  // add a correct non-dotted account id for the same account.
  ResetAccountTracker();

  SimulateTokenAvailable(kAccountKeyFooDotBar);
  SimulateTokenAvailable(kAccountKeyFooBar);
  ReturnAccountInfoFetchSuccess(kAccountKeyFooDotBar);
  ReturnAccountInfoFetchSuccess(kAccountKeyFooBar);

  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  std::vector<AccountInfo> infos = account_tracker()->GetAccounts();
  ASSERT_EQ(2u, infos.size());
  EXPECT_EQ(AccountKeyToEmail(kAccountKeyFooDotBar), infos[0].email);
  EXPECT_EQ(AccountKeyToEmail(kAccountKeyFooBar), infos[1].email);

  // Remove the bad account now from the token service to simulate that it
  // has been "fixed".
  SimulateTokenRevoked(kAccountKeyFooDotBar);

  // Instantiate a new tracker and validate that it has only one account, and
  // it is the correct non dotted one.
  ResetAccountTrackerNetworkDisabled();

  EXPECT_TRUE(account_fetcher()->IsAllUserInfoFetched());
  infos = account_tracker()->GetAccounts();
  ASSERT_EQ(1u, infos.size());
  EXPECT_EQ(AccountKeyToEmail(kAccountKeyFooBar), infos[0].email);
}

TEST_F(AccountTrackerServiceTest, MigrateAccountIdToGaiaId) {
  if (!AccountTrackerService::IsMigrationSupported())
    return;

  const std::string email_alpha = AccountKeyToEmail(kAccountKeyAlpha);
  const std::string gaia_alpha = AccountKeyToGaiaId(kAccountKeyAlpha);
  const std::string email_beta = AccountKeyToEmail(kAccountKeyBeta);
  const std::string gaia_beta = AccountKeyToGaiaId(kAccountKeyBeta);

  ListPrefUpdate update(prefs(), prefs::kAccountInfo);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("account_id", email_alpha);
  dict->SetString("email", email_alpha);
  dict->SetString("gaia", gaia_alpha);
  update->Append(std::move(dict));

  dict.reset(new base::DictionaryValue());
  dict->SetString("account_id", email_beta);
  dict->SetString("email", email_beta);
  dict->SetString("gaia", gaia_beta);
  update->Append(std::move(dict));

  base::HistogramTester tester;
  ResetAccountTracker();

  tester.ExpectBucketCount("Signin.AccountTracker.GaiaIdMigrationState",
                           AccountTrackerService::MIGRATION_IN_PROGRESS, 1);
  EXPECT_EQ(account_tracker()->GetMigrationState(),
            AccountTrackerService::MIGRATION_IN_PROGRESS);

  CoreAccountId gaia_alpha_account_id(gaia_alpha);
  AccountInfo account_info =
      account_tracker()->GetAccountInfo(gaia_alpha_account_id);
  EXPECT_EQ(account_info.account_id, gaia_alpha_account_id);
  EXPECT_EQ(account_info.gaia, gaia_alpha);
  EXPECT_EQ(account_info.email, email_alpha);

  account_info = account_tracker()->GetAccountInfo(CoreAccountId(gaia_beta));
  EXPECT_EQ(account_info.account_id, CoreAccountId(gaia_beta));
  EXPECT_EQ(account_info.gaia, gaia_beta);
  EXPECT_EQ(account_info.email, email_beta);

  std::vector<AccountInfo> accounts = account_tracker()->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
}

TEST_F(AccountTrackerServiceTest, CanNotMigrateAccountIdToGaiaId) {
  if (!AccountTrackerService::IsMigrationSupported())
    return;

  const std::string email_alpha = AccountKeyToEmail(kAccountKeyAlpha);
  const std::string gaia_alpha = AccountKeyToGaiaId(kAccountKeyAlpha);
  const std::string email_beta = AccountKeyToEmail(kAccountKeyBeta);

  ListPrefUpdate update(prefs(), prefs::kAccountInfo);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("account_id", email_alpha);
  dict->SetString("email", email_alpha);
  dict->SetString("gaia", gaia_alpha);
  update->Append(std::move(dict));

  dict.reset(new base::DictionaryValue());
  dict->SetString("account_id", email_beta);
  dict->SetString("email", email_beta);
  dict->SetString("gaia", "");
  update->Append(std::move(dict));

  base::HistogramTester tester;
  ResetAccountTracker();

  tester.ExpectBucketCount("Signin.AccountTracker.GaiaIdMigrationState",
                           AccountTrackerService::MIGRATION_NOT_STARTED, 1);
  EXPECT_EQ(account_tracker()->GetMigrationState(),
            AccountTrackerService::MIGRATION_NOT_STARTED);

  CoreAccountId email_alpha_account_id(email_alpha);
  AccountInfo account_info =
      account_tracker()->GetAccountInfo(email_alpha_account_id);
  EXPECT_EQ(account_info.account_id, email_alpha_account_id);
  EXPECT_EQ(account_info.gaia, gaia_alpha);
  EXPECT_EQ(account_info.email, email_alpha);

  CoreAccountId email_beta_account_id(email_beta);
  account_info = account_tracker()->GetAccountInfo(email_beta_account_id);
  EXPECT_EQ(account_info.account_id, email_beta_account_id);
  EXPECT_EQ(account_info.email, email_beta);

  std::vector<AccountInfo> accounts = account_tracker()->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
}

TEST_F(AccountTrackerServiceTest, GaiaIdMigrationCrashInTheMiddle) {
  if (!AccountTrackerService::IsMigrationSupported())
    return;

  const std::string email_alpha = AccountKeyToEmail(kAccountKeyAlpha);
  const std::string gaia_alpha = AccountKeyToGaiaId(kAccountKeyAlpha);
  const std::string email_beta = AccountKeyToEmail(kAccountKeyBeta);
  const std::string gaia_beta = AccountKeyToGaiaId(kAccountKeyBeta);

  ListPrefUpdate update(prefs(), prefs::kAccountInfo);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("account_id", email_alpha);
  dict->SetString("email", email_alpha);
  dict->SetString("gaia", gaia_alpha);
  update->Append(std::move(dict));

  dict.reset(new base::DictionaryValue());
  dict->SetString("account_id", email_beta);
  dict->SetString("email", email_beta);
  dict->SetString("gaia", gaia_beta);
  update->Append(std::move(dict));

  // Succeed miggrated account.
  dict.reset(new base::DictionaryValue());
  dict->SetString("account_id", gaia_alpha);
  dict->SetString("email", email_alpha);
  dict->SetString("gaia", gaia_alpha);
  update->Append(std::move(dict));

  base::HistogramTester tester;
  ResetAccountTracker();

  tester.ExpectBucketCount("Signin.AccountTracker.GaiaIdMigrationState",
                           AccountTrackerService::MIGRATION_IN_PROGRESS, 1);
  EXPECT_EQ(account_tracker()->GetMigrationState(),
            AccountTrackerService::MIGRATION_IN_PROGRESS);

  CoreAccountId gaia_alpha_account_id(gaia_alpha);
  AccountInfo account_info =
      account_tracker()->GetAccountInfo(gaia_alpha_account_id);
  EXPECT_EQ(account_info.account_id, gaia_alpha_account_id);
  EXPECT_EQ(account_info.gaia, gaia_alpha);
  EXPECT_EQ(account_info.email, email_alpha);

  CoreAccountId gaia_beta_account_id(gaia_beta);
  account_info = account_tracker()->GetAccountInfo(gaia_beta_account_id);
  EXPECT_EQ(account_info.account_id, gaia_beta_account_id);
  EXPECT_EQ(account_info.gaia, gaia_beta);
  EXPECT_EQ(account_info.email, email_beta);

  std::vector<AccountInfo> accounts = account_tracker()->GetAccounts();
  EXPECT_EQ(2u, accounts.size());

  ResetAccountTracker();

  tester.ExpectBucketCount("Signin.AccountTracker.GaiaIdMigrationState",
                           AccountTrackerService::MIGRATION_DONE, 1);
  EXPECT_EQ(account_tracker()->GetMigrationState(),
            AccountTrackerService::MIGRATION_DONE);

  account_info = account_tracker()->GetAccountInfo(gaia_alpha_account_id);
  EXPECT_EQ(account_info.account_id, gaia_alpha_account_id);
  EXPECT_EQ(account_info.gaia, gaia_alpha);
  EXPECT_EQ(account_info.email, email_alpha);

  account_info = account_tracker()->GetAccountInfo(gaia_beta_account_id);
  EXPECT_EQ(account_info.account_id, gaia_beta_account_id);
  EXPECT_EQ(account_info.gaia, gaia_beta);
  EXPECT_EQ(account_info.email, email_beta);

  accounts = account_tracker()->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
}

TEST_F(AccountTrackerServiceTest, ChildAccountBasic) {
  SimulateTokenAvailable(kAccountKeyChild);
  IssueAccessToken(kAccountKeyChild);
#if defined(OS_ANDROID)
  account_fetcher()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       true);
#else
  account_tracker()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       true);
#endif
  // Response was processed but observer is not notified as the account
  // state is invalid.
  EXPECT_TRUE(CheckAccountTrackerEvents({}));
  AccountInfo info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyChild));
  EXPECT_TRUE(info.is_child_account);
  SimulateTokenRevoked(kAccountKeyChild);
}

TEST_F(AccountTrackerServiceTest, ChildAccountUpdatedAndRevoked) {
  SimulateTokenAvailable(kAccountKeyChild);
  IssueAccessToken(kAccountKeyChild);
#if defined(OS_ANDROID)
  account_fetcher()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       false);
#else
  account_tracker()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       false);
#endif
  ReturnFetchResults(net::HTTP_OK,
                     GenerateValidTokenInfoResponse(kAccountKeyChild));
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
  AccountInfo info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyChild));
  EXPECT_FALSE(info.is_child_account);
  SimulateTokenRevoked(kAccountKeyChild);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(REMOVED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
}

TEST_F(AccountTrackerServiceTest, ChildAccountUpdatedAndRevokedWithUpdate) {
  SimulateTokenAvailable(kAccountKeyChild);
  IssueAccessToken(kAccountKeyChild);
#if defined(OS_ANDROID)
  account_fetcher()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       true);
#else
  account_tracker()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       true);
#endif
  ReturnFetchResults(net::HTTP_OK,
                     GenerateValidTokenInfoResponse(kAccountKeyChild));
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
  AccountInfo info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyChild));
  EXPECT_TRUE(info.is_child_account);
  SimulateTokenRevoked(kAccountKeyChild);
#if defined(OS_ANDROID)
  // On Android, is_child_account is set to false before removing it.
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
      TrackingEvent(REMOVED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
#else
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(REMOVED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
#endif
}

TEST_F(AccountTrackerServiceTest, ChildAccountUpdatedTwiceThenRevoked) {
  SimulateTokenAvailable(kAccountKeyChild);
  ReturnAccountInfoFetchSuccess(kAccountKeyChild);

  // Since the account state is already valid, this will notify the
  // observers for the second time.
#if defined(OS_ANDROID)
  account_fetcher()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       true);
#else
  account_tracker()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       true);
#endif
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
  SimulateTokenRevoked(kAccountKeyChild);
#if defined(OS_ANDROID)
  // On Android, is_child_account is set to false before removing it.
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
      TrackingEvent(REMOVED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
#else
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(REMOVED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
#endif
}

TEST_F(AccountTrackerServiceTest, ChildAccountGraduation) {
  SimulateTokenAvailable(kAccountKeyChild);
  IssueAccessToken(kAccountKeyChild);

  // Set and verify this is a child account.
#if defined(OS_ANDROID)
  account_fetcher()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       true);
#else
  account_tracker()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       true);
#endif
  AccountInfo info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyChild));
  EXPECT_TRUE(info.is_child_account);
  ReturnFetchResults(net::HTTP_OK,
                     GenerateValidTokenInfoResponse(kAccountKeyChild));
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));

  // Now simulate child account graduation.
#if defined(OS_ANDROID)
  account_fetcher()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       false);
#else
  account_tracker()->SetIsChildAccount(AccountKeyToAccountId(kAccountKeyChild),
                                       false);
#endif
  info = account_tracker()->GetAccountInfo(
      AccountKeyToAccountId(kAccountKeyChild));
  EXPECT_FALSE(info.is_child_account);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));

  SimulateTokenRevoked(kAccountKeyChild);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(REMOVED, AccountKeyToAccountId(kAccountKeyChild),
                    AccountKeyToGaiaId(kAccountKeyChild),
                    AccountKeyToEmail(kAccountKeyChild)),
  }));
}

TEST_F(AccountTrackerServiceTest, RemoveAccountBeforeImageFetchDone) {
  SimulateTokenAvailable(kAccountKeyAlpha);

  ReturnAccountInfoFetchSuccess(kAccountKeyAlpha);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(UPDATED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));

  SimulateTokenRevoked(kAccountKeyAlpha);
  ReturnAccountImageFetchFailure(kAccountKeyAlpha);
  EXPECT_TRUE(CheckAccountTrackerEvents({
      TrackingEvent(REMOVED, AccountKeyToAccountId(kAccountKeyAlpha),
                    AccountKeyToGaiaId(kAccountKeyAlpha),
                    AccountKeyToEmail(kAccountKeyAlpha)),
  }));
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS) && !defined(OS_IOS)
TEST_F(AccountTrackerServiceTest, AdvancedProtectionAccountBasic) {
  SimulateTokenAvailable(kAccountKeyAdvancedProtection);
  IssueAccessToken(kAccountKeyAdvancedProtection);

  const CoreAccountId account_id =
      AccountKeyToAccountId(kAccountKeyAdvancedProtection);
  account_tracker()->SetIsAdvancedProtectionAccount(account_id, true);
  AccountInfo info = account_tracker()->GetAccountInfo(account_id);
  EXPECT_TRUE(info.is_under_advanced_protection);

  account_tracker()->SetIsAdvancedProtectionAccount(account_id, false);
  info = account_tracker()->GetAccountInfo(account_id);
  EXPECT_FALSE(info.is_under_advanced_protection);

  SimulateTokenRevoked(kAccountKeyAdvancedProtection);
}

#endif

TEST_F(AccountTrackerServiceTest, CountOfLoadedAccounts_NoAccount) {
  base::HistogramTester tester;
  ResetAccountTracker();

  EXPECT_THAT(
      tester.GetAllSamples("Signin.AccountTracker.CountOfLoadedAccounts"),
      testing::ElementsAre(base::Bucket(0, 1)));
}

TEST_F(AccountTrackerServiceTest, CountOfLoadedAccounts_TwoAccounts) {
  const std::string email_alpha = AccountKeyToEmail(kAccountKeyAlpha);
  const std::string gaia_alpha = AccountKeyToGaiaId(kAccountKeyAlpha);
  const std::string email_beta = AccountKeyToEmail(kAccountKeyBeta);
  const std::string gaia_beta = AccountKeyToGaiaId(kAccountKeyBeta);

  ListPrefUpdate update(prefs(), prefs::kAccountInfo);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("account_id", email_alpha);
  dict->SetString("email", email_alpha);
  dict->SetString("gaia", gaia_alpha);
  update->Append(std::move(dict));

  dict.reset(new base::DictionaryValue());
  dict->SetString("account_id", email_beta);
  dict->SetString("email", email_beta);
  dict->SetString("gaia", gaia_beta);
  update->Append(std::move(dict));

  base::HistogramTester tester;
  ResetAccountTracker();

  EXPECT_THAT(
      tester.GetAllSamples("Signin.AccountTracker.CountOfLoadedAccounts"),
      testing::ElementsAre(base::Bucket(2, 1)));
}

TEST_F(AccountTrackerServiceTest, CountOfLoadedAccounts_TwoAccountsOneInvalid) {
  const std::string email_alpha = AccountKeyToEmail(kAccountKeyAlpha);
  const std::string gaia_alpha = AccountKeyToGaiaId(kAccountKeyAlpha);
  const std::string email_foobar = AccountKeyToEmail(kAccountKeyFooDotBar);
  const std::string gaia_foobar = AccountKeyToGaiaId(kAccountKeyFooDotBar);

  ListPrefUpdate update(prefs(), prefs::kAccountInfo);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("account_id", email_alpha);
  dict->SetString("email", email_alpha);
  dict->SetString("gaia", gaia_alpha);
  update->Append(std::move(dict));

  // This account is invalid because the account_id is a non-canonicalized
  // version of the email.
  dict.reset(new base::DictionaryValue());
  dict->SetString("account_id", email_foobar);
  dict->SetString("email", email_foobar);
  dict->SetString("gaia", gaia_foobar);
  update->Append(std::move(dict));

  base::HistogramTester tester;
  ResetAccountTracker();

  EXPECT_THAT(
      tester.GetAllSamples("Signin.AccountTracker.CountOfLoadedAccounts"),
      testing::ElementsAre(base::Bucket(1, 1)));
}
