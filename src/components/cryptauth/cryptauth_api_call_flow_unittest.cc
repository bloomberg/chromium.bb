// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/cryptauth_api_call_flow.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "components/cryptauth/network_request_error.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptauth {

namespace {

const char kSerializedRequestProto[] = "serialized_request_proto";
const char kSerializedResponseProto[] = "result_proto";
const char kRequestUrl[] = "https://googleapis.com/cryptauth/test";

}  // namespace

class CryptAuthApiCallFlowTest : public testing::Test {
 protected:
  CryptAuthApiCallFlowTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    flow_.SetPartialNetworkTrafficAnnotation(
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void StartApiCallFlow() {
    StartApiCallFlowWithRequest(kSerializedRequestProto);
  }

  void StartApiCallFlowWithRequest(const std::string& serialized_request) {
    flow_.Start(
        GURL(kRequestUrl), shared_factory_, "access_token", serialized_request,
        base::Bind(&CryptAuthApiCallFlowTest::OnResult, base::Unretained(this)),
        base::Bind(&CryptAuthApiCallFlowTest::OnError, base::Unretained(this)));
    // A pending fetch for the API request should be created.
    CheckCryptAuthHttpRequest(serialized_request);
  }

  void OnResult(const std::string& result) {
    EXPECT_FALSE(result_ || network_error_);
    result_.reset(new std::string(result));
  }

  void OnError(NetworkRequestError network_error) {
    EXPECT_FALSE(result_ || network_error_);
    network_error_.reset(new NetworkRequestError(network_error));
  }

  void CheckCryptAuthHttpRequest(const std::string& serialized_request) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *test_url_loader_factory_.pending_requests();
    ASSERT_EQ(1u, pending.size());
    const network::ResourceRequest& request = pending[0].request;
    EXPECT_EQ(GURL(kRequestUrl), request.url);
    EXPECT_EQ(serialized_request, network::GetUploadData(request));

    std::string content_type;
    EXPECT_TRUE(request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                          &content_type));
    EXPECT_EQ("application/x-protobuf", content_type);
  }

  // Responds to the current HTTP request. If the |error| is not |net::OK|, then
  // the |response_code| and |response_string| arguments will be ignored.
  void CompleteCurrentRequest(net::Error error,
                              int response_code,
                              const std::string& response_string) {
    network::URLLoaderCompletionStatus completion_status(error);
    network::ResourceResponseHead response_head;
    std::string content;
    if (error == net::OK) {
      response_head = network::CreateResourceResponseHead(
          static_cast<net::HttpStatusCode>(response_code));
      content = response_string;
    }
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrl), completion_status, response_head, content));
    scoped_task_environment_.RunUntilIdle();
    EXPECT_TRUE(result_ || network_error_);
  }

  std::unique_ptr<std::string> result_;
  std::unique_ptr<NetworkRequestError> network_error_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  CryptAuthApiCallFlow flow_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthApiCallFlowTest);
};

TEST_F(CryptAuthApiCallFlowTest, RequestSuccess) {
  StartApiCallFlow();
  CompleteCurrentRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(CryptAuthApiCallFlowTest, RequestFailure) {
  StartApiCallFlow();
  CompleteCurrentRequest(net::ERR_FAILED, 0, std::string());
  EXPECT_FALSE(result_);
  EXPECT_EQ(NetworkRequestError::kOffline, *network_error_);
}

TEST_F(CryptAuthApiCallFlowTest, RequestStatus500) {
  StartApiCallFlow();
  CompleteCurrentRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                         "CryptAuth Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ(NetworkRequestError::kInternalServerError, *network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(CryptAuthApiCallFlowTest, RequestWithNoBody) {
  StartApiCallFlowWithRequest(std::string());
  CompleteCurrentRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(CryptAuthApiCallFlowTest, ResponseWithNoBody) {
  StartApiCallFlow();
  CompleteCurrentRequest(net::OK, net::HTTP_OK, std::string());
  EXPECT_EQ(std::string(), *result_);
  EXPECT_FALSE(network_error_);
}

}  // namespace cryptauth
