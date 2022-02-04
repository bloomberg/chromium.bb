// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_discovery_interface.h"

#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/endpoint_fetcher/endpoint_fetcher.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using MockDiscoveryDeviceCallback = base::MockCallback<
    media_router::AccessCodeCastDiscoveryInterface::DiscoveryDeviceCallback>;

using AccessCodeCastDiscoveryInterface =
    media_router::AccessCodeCastDiscoveryInterface;

using MockEndpointFetcherCallback = base::MockCallback<EndpointFetcherCallback>;

using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

using AddSinkResultCode = access_code_cast::mojom::AddSinkResultCode;

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const char kMockPostData[] = "mock_post_data";
int64_t kMockTimeoutMs = 1000000;
const char kMockOAuthConsumerName[] = "mock_oauth_consumer_name";
const char kMockScope[] = "mock_scope";
const char kMockEndpoint[] = "https://my-endpoint.com";
const char kHttpMethod[] = "POST";
const char kContentType[] = "mock_content_type";
const char kEmail[] = "mock_email@gmail.com";

const char kMalformedResponse[] = "{{{foo_device:::broken}}";
const char kEmptyResponse[] = "";
const char kExpectedResponse[] = "{}";
const char kEndpointResponseSuccess[] =
    R"({
      "device": {
        "displayName": "test_device",
        "id": "1234",
        "deviceCapabilities": {
          "videoOut": true,
          "videoIn": true,
          "audioOut": true,
          "audioIn": true,
          "devMode": true
        },
        "networkInfo": {
          "hostName": "GoogleNet",
          "port": "666",
          "ipV4Address": "192.0.2.146",
          "ipV6Address": "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
        }
      }
    })";
const char kEndpointResponseFieldsMissing[] =
    R"({
      "device": {
        "displayName": "test_device",
        "id": "1234",
        "deviceCapabilities": {
          "videoOut": true,
          "videoIn": true,
          "audioOut": true,
          "audioIn": true,
          "devMode": true
        },
        "networkInfo": {
          "hostName": "GoogleNet",
          "port": "666",
        }
      }
    })";
// videoOut is a string instead of a bool in this test case
const char kEndpointResponseWrongDataTypes[] =
    R"({
      "device": {
        "displayName": "test_device",
        "id": "1234",
        "deviceCapabilities": {
          "videoOut": "true",
          "videoIn": true,
          "audioOut": true,
          "audioIn": true,
          "devMode": true
        },
        "networkInfo": {
          "hostName": "GoogleNet",
          "port": "666",
          "ipV4Address": "192.0.2.146",
          "ipV6Address": "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
        }
      }
    })";

}  // namespace

MATCHER_P(DiscoveryDeviceProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  // Must extract the actual proto from the optional<DiscoveryDevice>
  CHECK(arg.has_value());
  DiscoveryDevice arg_value = arg.value();
  arg_value.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class AccessCodeCastDiscoveryInterfaceTest : public testing::Test {
 protected:
  AccessCodeCastDiscoveryInterfaceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  AccessCodeCastDiscoveryInterfaceTest(
      const AccessCodeCastDiscoveryInterfaceTest&
          access_code_cast_discovery_interface_test) = delete;
  AccessCodeCastDiscoveryInterfaceTest& operator=(
      const AccessCodeCastDiscoveryInterfaceTest&
          access_code_cast_discovery_interface_test) = delete;

  ~AccessCodeCastDiscoveryInterfaceTest() override {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    Profile* profile = profile_manager()->CreateTestingProfile("foo_email");

    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    endpoint_fetcher_ = std::make_unique<EndpointFetcher>(
        kMockOAuthConsumerName, GURL(kMockEndpoint), kHttpMethod, kContentType,
        std::vector<std::string>{kMockScope}, kMockTimeoutMs, kMockPostData,
        TRAFFIC_ANNOTATION_FOR_TESTS, test_url_loader_factory,
        identity_test_env_.identity_manager());

    discovery_interface_ = std::make_unique<AccessCodeCastDiscoveryInterface>(
        profile, "123456", std::move(endpoint_fetcher_));

    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();
    SignIn();
  }

  void SetEndpointFetcherMockResponse(const GURL& request_url,
                                      const std::string& response_data,
                                      net::HttpStatusCode response_code,
                                      net::Error error) {
    auto head = network::mojom::URLResponseHead::New();
    std::string headers(base::StringPrintf(
        "HTTP/1.1 %d %s\nContent-type: application/json\n\n",
        static_cast<int>(response_code), GetHttpReasonPhrase(response_code)));
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    head->mime_type = "application/json";
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = response_data.size();
    test_url_loader_factory_.AddResponse(request_url, std::move(head),
                                         response_data, status);
  }

  void SignIn() {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail,
                                                   signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  MockEndpointFetcherCallback& endpoint_fetcher_callback() {
    return mock_callback_;
  }

  AccessCodeCastDiscoveryInterface* stub_interface() {
    return discovery_interface_.get();
  }

  EndpointFetcher* endpoint_fetcher() { return endpoint_fetcher_.get(); }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  MockEndpointFetcherCallback mock_callback_;
  std::unique_ptr<AccessCodeCastDiscoveryInterface> discovery_interface_;
  TestingProfileManager profile_manager_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<EndpointFetcher> endpoint_fetcher_;
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
};

TEST_F(AccessCodeCastDiscoveryInterfaceTest,
       ServerResponseAuthenticationError) {
  // Test to validate that an authentication error is propagated from the
  // discovery interface.
  identity_test_env().SetAutomaticIssueOfAccessTokens(false);

  MockDiscoveryDeviceCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(Eq(absl::nullopt), AddSinkResultCode::AUTH_ERROR));

  stub_interface()->ValidateDiscoveryAccessCode(mock_callback.Get());
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccessCodeCastDiscoveryInterfaceTest, ServerError) {
  // Test to validate that a server error is propagated from the discovery
  // interface.
  SetEndpointFetcherMockResponse(GURL(kMockEndpoint), kExpectedResponse,
                                 net::HTTP_BAD_REQUEST, net::ERR_FAILED);

  MockDiscoveryDeviceCallback mock_callback;

  EXPECT_CALL(mock_callback,
              Run(Eq(absl::nullopt), AddSinkResultCode::SERVER_ERROR));

  stub_interface()->ValidateDiscoveryAccessCode(mock_callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccessCodeCastDiscoveryInterfaceTest, ServerResponseMalformedError) {
  // Test to validate that a malformed server response is propagated from the
  // discovery interface.
  SetEndpointFetcherMockResponse(GURL(kMockEndpoint), kMalformedResponse,
                                 net::HTTP_OK, net::OK);

  MockDiscoveryDeviceCallback mock_callback;

  EXPECT_CALL(mock_callback,
              Run(Eq(absl::nullopt), AddSinkResultCode::RESPONSE_MALFORMED));

  stub_interface()->ValidateDiscoveryAccessCode(mock_callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccessCodeCastDiscoveryInterfaceTest, ServerResponseEmptyError) {
  // Test to validate that an empty server response is propagated from the
  // discovery interface.
  SetEndpointFetcherMockResponse(GURL(kMockEndpoint), kEmptyResponse,
                                 net::HTTP_OK, net::OK);

  MockDiscoveryDeviceCallback mock_callback;

  EXPECT_CALL(mock_callback,
              Run(Eq(absl::nullopt), AddSinkResultCode::RESPONSE_MALFORMED));

  stub_interface()->ValidateDiscoveryAccessCode(mock_callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccessCodeCastDiscoveryInterfaceTest, ServerResponseSucess) {
  // Test to validate that a successful server response is propagated from
  // the discovery interface and all fields are set in the returned proto.
  SetEndpointFetcherMockResponse(GURL(kMockEndpoint), kEndpointResponseSuccess,
                                 net::HTTP_OK, net::OK);

  MockDiscoveryDeviceCallback mock_callback;

  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto();

  EXPECT_CALL(mock_callback,
              Run(DiscoveryDeviceProtoEquals(discovery_device_proto),
                  AddSinkResultCode::OK));

  stub_interface()->ValidateDiscoveryAccessCode(mock_callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccessCodeCastDiscoveryInterfaceTest, FieldsMissingInResponse) {
  // Test to validate that a server response with missing fields is
  // propagated from the discovery interface.
  SetEndpointFetcherMockResponse(GURL(kMockEndpoint),
                                 kEndpointResponseFieldsMissing, net::HTTP_OK,
                                 net::OK);

  MockDiscoveryDeviceCallback mock_callback;

  EXPECT_CALL(mock_callback,
              Run(Eq(absl::nullopt), AddSinkResultCode::RESPONSE_MALFORMED));

  stub_interface()->ValidateDiscoveryAccessCode(mock_callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccessCodeCastDiscoveryInterfaceTest, WrongDataTypesInResponse) {
  // Test to validate that a server response with wrong data types is
  // propagated from the discovery interface.
  SetEndpointFetcherMockResponse(GURL(kMockEndpoint),
                                 kEndpointResponseWrongDataTypes, net::HTTP_OK,
                                 net::OK);

  MockDiscoveryDeviceCallback mock_callback;

  EXPECT_CALL(mock_callback,
              Run(Eq(absl::nullopt), AddSinkResultCode::RESPONSE_MALFORMED));

  stub_interface()->ValidateDiscoveryAccessCode(mock_callback.Get());
  base::RunLoop().RunUntilIdle();
}
