// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/http_ice_config_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/protocol/ice_config.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

namespace {

const char kTestResponse[] =
    "{"
    "  \"lifetimeDuration\": \"43200.000s\","
    "  \"iceServers\": ["
    "    {"
    "      \"urls\": ["
    "        \"turns:the_server.com\""
    "      ],"
    "      \"username\": \"123\","
    "      \"credential\": \"abc\""
    "    },"
    "    {"
    "      \"urls\": ["
    "        \"stun:stun_server.com:18344\""
    "      ]"
    "    }"
    "  ]"
    "}";
const char kTestOAuthToken[] = "TestOAuthToken";

const char kAuthHeaderBearer[] = "Bearer ";

}  // namespace

static const char kTestUrl[] = "http://host/ice_config";

class HttpIceConfigRequestTest : public testing::Test {
 public:
  HttpIceConfigRequestTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        url_request_factory_(test_shared_url_loader_factory_) {}

  void OnResult(const IceConfig& config) {
    received_config_ = std::make_unique<IceConfig>(config);
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  ChromiumUrlRequestFactory url_request_factory_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<HttpIceConfigRequest> request_;
  std::unique_ptr<IceConfig> received_config_;
};

TEST_F(HttpIceConfigRequestTest, Parse) {
  test_url_loader_factory_.AddResponse(kTestUrl, kTestResponse);

  request_.reset(
      new HttpIceConfigRequest(&url_request_factory_, kTestUrl, nullptr));
  request_->Send(
      base::Bind(&HttpIceConfigRequestTest::OnResult, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(received_config_->is_null());

  EXPECT_EQ(1U, received_config_->turn_servers.size());
  EXPECT_EQ(1U, received_config_->stun_servers.size());
}

TEST_F(HttpIceConfigRequestTest, InvalidConfig) {
  test_url_loader_factory_.AddResponse(kTestUrl, "ERROR");

  request_.reset(
      new HttpIceConfigRequest(&url_request_factory_, kTestUrl, nullptr));
  request_->Send(
      base::Bind(&HttpIceConfigRequestTest::OnResult, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(received_config_->is_null());
}

TEST_F(HttpIceConfigRequestTest, FailedRequest) {
  test_url_loader_factory_.AddResponse(kTestUrl, std::string(),
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  request_.reset(
      new HttpIceConfigRequest(&url_request_factory_, kTestUrl, nullptr));
  request_->Send(
      base::Bind(&HttpIceConfigRequestTest::OnResult, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(received_config_->is_null());
}

TEST_F(HttpIceConfigRequestTest, Authentication) {
  test_url_loader_factory_.AddResponse(kTestUrl, kTestResponse);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(
            request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
        std::string auth_header;
        request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization,
                                  &auth_header);
        EXPECT_EQ(auth_header,
                  std::string(kAuthHeaderBearer) + kTestOAuthToken);
      }));

  FakeOAuthTokenGetter token_getter(OAuthTokenGetter::SUCCESS,
                                    "user@example.com", kTestOAuthToken);
  request_ = std::make_unique<HttpIceConfigRequest>(&url_request_factory_,
                                                    kTestUrl, &token_getter);
  request_->Send(
      base::Bind(&HttpIceConfigRequestTest::OnResult, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(received_config_);

  EXPECT_EQ(1U, received_config_->turn_servers.size());
  EXPECT_EQ(1U, received_config_->stun_servers.size());
}

}  // namespace protocol
}  // namespace remoting
