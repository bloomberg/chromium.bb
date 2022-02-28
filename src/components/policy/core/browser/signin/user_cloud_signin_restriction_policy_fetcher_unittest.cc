// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/proto/secure_connect.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction";
}

namespace policy {

class UserCloudSigninRestrictionPolicyFetcherTest : public ::testing::Test {
 public:
  UserCloudSigninRestrictionPolicyFetcherTest()
      : policy_fetcher_(nullptr, nullptr) {}

  UserCloudSigninRestrictionPolicyFetcher* policy_fetcher() {
    return &policy_fetcher_;
  }
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_env_;
  UserCloudSigninRestrictionPolicyFetcher policy_fetcher_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest, ReturnsValueFromBody) {
  network::TestURLLoaderFactory url_loader_factory;
  enterprise_management::GetManagedAccountsSigninRestrictionResponse
      expected_response_proto;
  std::string response;
  expected_response_proto.set_policy_value("primary_account");
  expected_response_proto.SerializeToString(&response);
  url_loader_factory.AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl,
      std::move(response));

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_response_proto.policy_value(), result);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueIfServerError) {
  network::TestURLLoaderFactory url_loader_factory;
  enterprise_management::GetManagedAccountsSigninRestrictionResponse
      expected_response_proto;
  std::string response;
  expected_response_proto.set_policy_value("primary_account");
  expected_response_proto.set_has_error(true);
  expected_response_proto.SerializeToString(&response);
  url_loader_factory.AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl,
      std::move(response));

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::string(), result);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueIfNetworkError) {
  network::TestURLLoaderFactory url_loader_factory;
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  url_loader_factory.AddResponse(
      GURL(kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl),
      /*head=*/std::move(head), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::string(), result);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueIfHTTPError) {
  network::TestURLLoaderFactory url_loader_factory;
  url_loader_factory.AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl, std::string(),
      net::HTTP_BAD_GATEWAY);

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::string(), result);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueInResponseNotParsable) {
  network::TestURLLoaderFactory url_loader_factory;
  url_loader_factory.AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl, "bad");

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::string(), result);
}

}  // namespace policy
