// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/user_cloud_signin_restriction_policy_fetcher_chromeos.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

const char kSecureConnectApiGetSecondaryGoogleAccountUsageUrl[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction?policy_name="
    "SecondaryGoogleAccountUsage";
const char kFakeAccessToken[] = "fake-access-token";
const char kFakeRefreshToken[] = "fake-refresh-token";
const char kFakeEnterpriseAccount[] = "alice@acme.com";
const char kFakeEnterpriseDomain[] = "acme.com";
const char kFakeGmailAccount[] = "example@gmail.com";
const char kFakeNonEnterpriseAccount[] = "alice@consumeremail.com";
const char kBadResponseBody[] = "bad-response-body";
static const char kUserInfoResponse[] =
    "{"
    "  \"hd\": \"acme.com\""
    "}";

// A mock access token fetcher. Calls the appropriate error or success callback,
// depending on the `error` provided in the constructor.
class MockAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  MockAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                         const GoogleServiceAuthError& error)
      : OAuth2AccessTokenFetcher(consumer), error_(error) {}

  MockAccessTokenFetcher(const MockAccessTokenFetcher&) = delete;
  MockAccessTokenFetcher& operator=(const MockAccessTokenFetcher&) = delete;

  ~MockAccessTokenFetcher() override = default;

  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override {
    if (error_ != GoogleServiceAuthError::AuthErrorNone()) {
      FireOnGetTokenFailure(error_);
      return;
    }

    // Send a success response.
    OAuth2AccessTokenConsumer::TokenResponse::Builder builder;
    builder.WithAccessToken(kFakeAccessToken);
    builder.WithRefreshToken(kFakeRefreshToken);
    builder.WithExpirationTime(base::Time::Now() + base::Hours(1));
    builder.WithIdToken("id_token");
    FireOnGetTokenSuccess(builder.build());
  }

  void CancelRequest() override {}

 private:
  const GoogleServiceAuthError error_;
};

}  // namespace

class UserCloudSigninRestrictionPolicyFetcherChromeOSTest
    : public ::testing::Test {
 public:
  UserCloudSigninRestrictionPolicyFetcherChromeOSTest() = default;

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &url_loader_factory_);
  }

 protected:
  // Get policy value for SecondaryGoogleAccountUsage.
  void GetSecondaryGoogleAccountUsageBlocking(
      UserCloudSigninRestrictionPolicyFetcherChromeOS* restriction_fetcher,
      std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher) {
    base::RunLoop run_loop;
    restriction_fetcher->GetSecondaryGoogleAccountUsage(
        std::move(access_token_fetcher),
        base::BindLambdaForTesting(
            [this, &run_loop](
                UserCloudSigninRestrictionPolicyFetcherChromeOS::Status st,
                absl::optional<std::string> res, const std::string& hd) {
              this->policy_result_ = res;
              this->status_ = st;
              this->hosted_domain_ = hd;
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  const std::string& oauth_user_info_url() const {
    return GaiaUrls::GetInstance()->oauth_user_info_url().spec();
  }

  // Check base/test/task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  UserCloudSigninRestrictionPolicyFetcherChromeOS::Status status_ =
      UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::kUnknownError;
  absl::optional<std::string> policy_result_;
  std::string hosted_domain_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyValueSucceeds) {
  // Set API response.
  base::Value expected_response(base::Value::Type::DICTIONARY);
  expected_response.SetStringKey("policyValue", "primary_account_signin");
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, std::move(response));
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_TRUE(policy_result_);
  EXPECT_EQ(policy_result_.value(), "primary_account_signin");
  EXPECT_EQ(status_,
            UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::kSuccess);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingUserInfoFailsForNetworkConnectionErrors) {
  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Fake a failed UserInfo fetch.
  url_loader_factory_.AddResponse(oauth_user_info_url(), std::string(),
                                  net::HTTP_INTERNAL_SERVER_ERROR);

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_, UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::
                         kGetUserInfoError);
  EXPECT_EQ(hosted_domain_, std::string());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingAccessTokenFailsForNetworkConnectionErrors) {
  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create an access token fetcher that simulates a network connection error.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
          &restriction_fetcher,
          GoogleServiceAuthError::FromConnectionError(/*error=*/0));

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(
      status_,
      UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::kGetTokenError);
  EXPECT_EQ(hosted_domain_, std::string());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyValueFailsForNetworkErrors) {
  // Fake network error.
  url_loader_factory_.AddResponse(
      GURL(kSecureConnectApiGetSecondaryGoogleAccountUsageUrl),
      /*head=*/network::mojom::URLResponseHead::New(),
      /*content=*/std::string(),
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(
      status_,
      UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::kNetworkError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyValueFailsForHTTPErrors) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, std::string(),
      net::HTTP_BAD_GATEWAY);
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(
      status_,
      UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::kHttpError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyReturnsEmptyPolicyForResponsesNotParsable) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, kBadResponseBody);
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_, UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::
                         kParsingResponseError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyReturnsEmptyPolicyForConsumerGmailAccounts) {
  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeGmailAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_, UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::
                         kUnsupportedAccountTypeError);
  EXPECT_EQ(hosted_domain_, std::string());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyReturnsEmptyPolicyForNonEnterpriseAccounts) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, kBadResponseBody);
  // Simulate an empty response body for non enterprise accounts.
  url_loader_factory_.AddResponse(oauth_user_info_url(), /*content=*/"{}");

  UserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeNonEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_, UserCloudSigninRestrictionPolicyFetcherChromeOS::Status::
                         kUnsupportedAccountTypeError);
  EXPECT_EQ(hosted_domain_, std::string());
}

}  // namespace ash
