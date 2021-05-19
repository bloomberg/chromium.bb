// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_request.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::Ne;
using testing::Pointee;

const char kAccessToken[] = "access_token";
const char kRequestUrl[] = "https://test.com/test";
const char kRequestUrlWithAlternateOutputProto[] =
    "https://test.com/test?alt=proto";
const char kResponseBody[] = "response_body";

MATCHER(HasValidAccessToken, "") {
  const network::TestURLLoaderFactory::PendingRequest& pending_request = arg;
  std::string access_token_header;
  pending_request.request.headers.GetHeader("Authorization",
                                            &access_token_header);
  return access_token_header == base::StringPrintf("Bearer %s", kAccessToken);
}

class FakeTrustedVaultAccessTokenFetcher
    : public TrustedVaultAccessTokenFetcher {
 public:
  explicit FakeTrustedVaultAccessTokenFetcher(
      const base::Optional<std::string>& access_token)
      : access_token_(access_token) {}
  ~FakeTrustedVaultAccessTokenFetcher() override = default;

  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override {
    base::Optional<signin::AccessTokenInfo> access_token_info;
    if (access_token_) {
      access_token_info = signin::AccessTokenInfo(
          *access_token_, base::Time::Now() + base::TimeDelta::FromHours(1),
          /*id_token=*/std::string());
    }
    std::move(callback).Run(access_token_info);
  }

 private:
  const base::Optional<std::string> access_token_;
};

class TrustedVaultRequestTest : public testing::Test {
 public:
  TrustedVaultRequestTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  std::unique_ptr<TrustedVaultRequest> StartNewRequestWithAccessToken(
      const base::Optional<std::string>& access_token,
      TrustedVaultRequest::HttpMethod http_method,
      const base::Optional<std::string>& request_body,
      TrustedVaultRequest::CompletionCallback completion_callback) {
    const CoreAccountId account_id = CoreAccountId::FromEmail("user@gmail.com");
    FakeTrustedVaultAccessTokenFetcher access_token_fetcher(access_token);

    auto request = std::make_unique<TrustedVaultRequest>(
        http_method, GURL(kRequestUrl), request_body);
    request->FetchAccessTokenAndSendRequest(
        account_id, shared_url_loader_factory_, &access_token_fetcher,
        std::move(completion_callback));
    return request;
  }

  bool RespondToHttpRequest(
      net::Error error,
      base::Optional<net::HttpStatusCode> response_http_code,
      const std::string& response_body) {
    network::mojom::URLResponseHeadPtr response_head;
    if (response_http_code.has_value()) {
      response_head = network::CreateURLResponseHead(*response_http_code);
    } else {
      response_head = network::mojom::URLResponseHead::New();
    }
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrlWithAlternateOutputProto),
        network::URLLoaderCompletionStatus(error), std::move(response_head),
        response_body);
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest() {
    return test_url_loader_factory_.GetPendingRequest(/*index=*/0);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

}  // namespace

TEST_F(TrustedVaultRequestTest, ShouldSendGetRequestAndHandleSuccess) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/base::nullopt, completion_callback.Get());

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, Pointee(HasValidAccessToken()));

  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("GET"));
  EXPECT_THAT(resource_request.url,
              Eq(GURL(kRequestUrlWithAlternateOutputProto)));
  EXPECT_THAT(network::GetUploadData(resource_request), IsEmpty());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(
      completion_callback,
      Run(TrustedVaultRequest::HttpStatus::kSuccess, Eq(kResponseBody)));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_OK, kResponseBody));
}

TEST_F(TrustedVaultRequestTest,
       ShouldSendPostRequestWithoutPayloadAndHandleSuccess) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kPost,
      /*request_body=*/base::nullopt, completion_callback.Get());

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, Pointee(HasValidAccessToken()));

  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GURL(kRequestUrlWithAlternateOutputProto)));
  EXPECT_THAT(network::GetUploadData(resource_request), IsEmpty());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(
      completion_callback,
      Run(TrustedVaultRequest::HttpStatus::kSuccess, Eq(kResponseBody)));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_OK, kResponseBody));
}

TEST_F(TrustedVaultRequestTest,
       ShouldSendPostRequestWithPayloadAndHandleSuccess) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  const std::string kRequestBody = "Request body";
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kPost, kRequestBody,
      completion_callback.Get());

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, Pointee(HasValidAccessToken()));

  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GURL(kRequestUrlWithAlternateOutputProto)));
  EXPECT_THAT(network::GetUploadData(resource_request), Eq(kRequestBody));

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(
      completion_callback,
      Run(TrustedVaultRequest::HttpStatus::kSuccess, Eq(kResponseBody)));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_OK, kResponseBody));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleNetworkFailures) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/base::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kOtherError, _));
  EXPECT_TRUE(RespondToHttpRequest(net::ERR_FAILED, base::nullopt,
                                   /*response_body=*/std::string()));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleHttpErrors) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/base::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kOtherError, _));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                                   /*response_body=*/""));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleNotFoundStatus) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/base::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kNotFound, _));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_NOT_FOUND,
                                   /*response_body=*/""));
}

TEST_F(TrustedVaultRequestTest, ShouldHandlePreconditionFailedStatus) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/base::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kFailedPrecondition, _));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_PRECONDITION_FAILED,
                                   /*response_body=*/""));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleAccessTokenFetchingFailures) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  // Access token fetching failure propagated immediately in this test, so
  // |completion_callback| should be called immediately as well.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kOtherError, _));
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      /*access_token=*/base::nullopt, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/base::nullopt, completion_callback.Get());
}

}  // namespace syncer
