// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/http_ice_config_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "remoting/base/url_request.h"
#include "remoting/protocol/ice_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

namespace {

class FakeUrlRequest : public UrlRequest {
 public:
  FakeUrlRequest(const Result& result) : result_(result) {}
  ~FakeUrlRequest() override {}

  // UrlRequest interface.
  void AddHeader(const std::string& value) override { NOTREACHED(); }

  void SetPostData(const std::string& content_type,
                   const std::string& post_data) override {
    EXPECT_EQ("application/json", content_type);
    EXPECT_EQ("", post_data);
  }

  void Start(const OnResultCallback& on_result_callback) override {
    on_result_callback.Run(result_);
  }

 private:
  Result result_;
};

class FakeUrlRequestFactory : public UrlRequestFactory {
 public:
  FakeUrlRequestFactory() {}
  ~FakeUrlRequestFactory() override {}

  void SetResult(const std::string& url, const UrlRequest::Result& result) {
    results_[url] = result;
  }

  // UrlRequestFactory interface.
  std::unique_ptr<UrlRequest> CreateUrlRequest(
      UrlRequest::Type type,
      const std::string& url) override {
    EXPECT_EQ(UrlRequest::Type::POST, type);
    CHECK(results_.count(url));
    return base::MakeUnique<FakeUrlRequest>(results_[url]);
  }

  std::map<std::string, UrlRequest::Result> results_;
};

}  // namespace

static const char kTestUrl[] = "http://host/ice_config";

class HttpIceConfigRequestTest : public testing::Test {
 public:
  void OnResult(const IceConfig& config) {
    received_config_ = base::MakeUnique<IceConfig>(config);
  }

 protected:
  FakeUrlRequestFactory url_request_factory_;
  std::unique_ptr<HttpIceConfigRequest> request_;
  std::unique_ptr<IceConfig> received_config_;
};

TEST_F(HttpIceConfigRequestTest, Parse) {
  const char kTestResponse[] =
      "{"
      "  \"lifetimeDuration\": \"43200.000s\","
      "  \"iceServers\": ["
      "    {"
      "      \"urls\": ["
      "        \"turn:8.8.8.8:19234\","
      "        \"turn:[2001:4860:4860::8888]:333\","
      "        \"turn:[2001:4860:4860::8888]\","
      "        \"turn:[2001:4860:4860::8888]:333?transport=tcp\","
      "        \"turns:the_server.com\","
      "        \"turns:the_server.com?transport=udp\""
      "      ],"
      "      \"username\": \"123\","
      "      \"credential\": \"abc\""
      "    },"
      "    {"
      "      \"urls\": ["
      "        \"stun:stun_server.com:18344\","
      "        \"stun:1.2.3.4\""
      "      ]"
      "    }"
      "  ]"
      "}";
  url_request_factory_.SetResult(kTestUrl,
                                 UrlRequest::Result(200, kTestResponse));
  request_.reset(new HttpIceConfigRequest(&url_request_factory_, kTestUrl));
  request_->Send(
      base::Bind(&HttpIceConfigRequestTest::OnResult, base::Unretained(this)));
  ASSERT_FALSE(received_config_->is_null());

  // lifetimeDuration in the config is set to 12 hours. HttpIceConfigRequest
  // substracts 1 hour. Verify that |expiration_time| is in the rage from 10 to
  // 12 hours from now.
  EXPECT_TRUE(base::Time::Now() + base::TimeDelta::FromHours(10) <
              received_config_->expiration_time);
  EXPECT_TRUE(received_config_->expiration_time <
              base::Time::Now() + base::TimeDelta::FromHours(12));

  EXPECT_EQ(6U, received_config_->turn_servers.size());
  EXPECT_TRUE(cricket::RelayServerConfig("8.8.8.8", 19234, "123", "abc",
                                         cricket::PROTO_UDP, false) ==
              received_config_->turn_servers[0]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 333, "123",
                                         "abc", cricket::PROTO_UDP, false) ==
              received_config_->turn_servers[1]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 3478, "123",
                                         "abc", cricket::PROTO_UDP, false) ==
              received_config_->turn_servers[2]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 333, "123",
                                         "abc", cricket::PROTO_TCP, false) ==
              received_config_->turn_servers[3]);
  EXPECT_TRUE(cricket::RelayServerConfig("the_server.com", 5349, "123", "abc",
                                         cricket::PROTO_TCP, true) ==
              received_config_->turn_servers[4]);
  EXPECT_TRUE(cricket::RelayServerConfig("the_server.com", 5349, "123", "abc",
                                         cricket::PROTO_UDP, true) ==
              received_config_->turn_servers[5]);

  EXPECT_EQ(2U, received_config_->stun_servers.size());
  EXPECT_EQ(rtc::SocketAddress("stun_server.com", 18344),
            received_config_->stun_servers[0]);
  EXPECT_EQ(rtc::SocketAddress("1.2.3.4", 3478),
            received_config_->stun_servers[1]);
}

// Verify that we can still proceed if some servers cannot be parsed.
TEST_F(HttpIceConfigRequestTest, ParsePartiallyInvalid) {
  const char kTestResponse[] =
      "{"
      "  \"lifetimeDuration\": \"43200.000s\","
      "  \"iceServers\": ["
      "    {"
      "      \"urls\": ["
      "        \"InvalidURL\","
      "        \"turn:[2001:4860:4860::8888]:333\""
      "      ],"
      "      \"username\": \"123\","
      "      \"credential\": \"abc\""
      "    },"
      "    \"42\""
      "  ]"
      "}";
  url_request_factory_.SetResult(kTestUrl,
                                 UrlRequest::Result(200, kTestResponse));
  request_.reset(new HttpIceConfigRequest(&url_request_factory_, kTestUrl));
  request_->Send(
      base::Bind(&HttpIceConfigRequestTest::OnResult, base::Unretained(this)));
  ASSERT_FALSE(received_config_->is_null());

  // Config should be already expired because it couldn't be parsed.
  EXPECT_TRUE(received_config_->expiration_time < base::Time::Now());

  EXPECT_EQ(1U, received_config_->turn_servers.size());
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 333, "123",
                                         "abc", cricket::PROTO_UDP, false) ==
              received_config_->turn_servers[0]);
}

TEST_F(HttpIceConfigRequestTest, NotParseable) {
  url_request_factory_.SetResult(kTestUrl,
                                 UrlRequest::Result(200, "ERROR"));
  request_.reset(new HttpIceConfigRequest(&url_request_factory_, kTestUrl));
  request_->Send(
      base::Bind(&HttpIceConfigRequestTest::OnResult, base::Unretained(this)));
  EXPECT_TRUE(received_config_->is_null());
}

TEST_F(HttpIceConfigRequestTest, FailedRequest) {
  url_request_factory_.SetResult(kTestUrl, UrlRequest::Result::Failed());
  request_.reset(new HttpIceConfigRequest(&url_request_factory_, kTestUrl));
  request_->Send(
      base::Bind(&HttpIceConfigRequestTest::OnResult, base::Unretained(this)));
  EXPECT_TRUE(received_config_->is_null());
}

}  // namespace protocol
}  // namespace remoting
