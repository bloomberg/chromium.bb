// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/web_push_sender.h"

#include "base/base64.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/ec_private_key.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// An ASN.1-encoded PrivateKeyInfo block from PKCS #8.
const char kPrivateKey[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgS8wRbDOWz0lKExvIVQiRKtPAP8"
    "dgHUHAw5gyOd5d4jKhRANCAARZb49Va5MD/KcWtc0oiWc2e8njBDtQzj0mzcOl1fDSt16Pvu6p"
    "fTU3MTWnImDNnkPxtXm58K7Uax8jFxA4TeXJ";

}  // namespace

namespace gcm {

WebPushMessage CreateMessage() {
  WebPushMessage message;
  message.time_to_live = 3600;
  message.payload = "payload";
  return message;
}

class WebPushSenderTest : public testing::Test {
 public:
  WebPushSenderTest();
  ~WebPushSenderTest() override;

  void SetUp() override;

  WebPushSender* sender() { return sender_.get(); }
  network::TestURLLoaderFactory& loader() { return test_url_loader_factory_; }

  void OnMessageSent(SendWebPushMessageResult* result_out,
                     base::Optional<std::string>* message_id_out,
                     SendWebPushMessageResult result,
                     base::Optional<std::string> message_id) {
    *result_out = result;
    *message_id_out = message_id;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<WebPushSender> sender_;
};

WebPushSenderTest::WebPushSenderTest() = default;
WebPushSenderTest::~WebPushSenderTest() = default;

void WebPushSenderTest::SetUp() {
  sender_ = std::make_unique<WebPushSender>(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_));
}

TEST_F(WebPushSenderTest, SendMessageTest) {
  std::string private_key_info;
  ASSERT_TRUE(base::Base64Decode(kPrivateKey, &private_key_info));
  std::unique_ptr<crypto::ECPrivateKey> private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(std::vector<uint8_t>(
          private_key_info.begin(), private_key_info.end()));
  ASSERT_TRUE(private_key);

  SendWebPushMessageResult result;
  base::Optional<std::string> message_id;
  sender()->SendMessage(
      "fcm_token", private_key.get(), CreateMessage(),
      base::BindOnce(&WebPushSenderTest::OnMessageSent, base::Unretained(this),
                     &result, &message_id));

  ASSERT_EQ(loader().NumPending(), 1);

  network::TestURLLoaderFactory::PendingRequest* pendingRequest =
      loader().GetPendingRequest(0);
  ASSERT_EQ("https://fcm.googleapis.com/fcm/send/fcm_token",
            pendingRequest->request.url);
  ASSERT_EQ("POST", pendingRequest->request.method);

  net::HttpRequestHeaders headers = pendingRequest->request.headers;
  std::string auth_header, time_to_live, encoding;

  ASSERT_TRUE(
      headers.GetHeader(net::HttpRequestHeaders::kAuthorization, &auth_header));
  const std::string expected_header =
      "vapid "
      "t=([a-zA-Z0-9_-])+\\.([a-zA-Z0-9_-])+\\.([a-zA-Z0-9_-])+, "
      "k=([a-zA-Z0-9_-])+";
  ASSERT_TRUE(re2::RE2::FullMatch(auth_header, expected_header));

  ASSERT_TRUE(headers.GetHeader("TTL", &time_to_live));
  ASSERT_EQ("3600", time_to_live);

  ASSERT_TRUE(headers.GetHeader("content-encoding", &encoding));
  ASSERT_EQ("aes128gcm", encoding);

  const std::vector<network::DataElement>* body_elements =
      pendingRequest->request.request_body->elements();
  ASSERT_EQ(1UL, body_elements->size());
  const network::DataElement& body = body_elements->back();
  ASSERT_EQ("payload", std::string(body.bytes(), body.length()));

  network::ResourceResponseHead response_head =
      network::CreateResourceResponseHead(net::HTTP_OK);
  response_head.headers->AddHeader(
      "location:https://fcm.googleapis.com/message_id");

  loader().SimulateResponseForPendingRequest(
      pendingRequest->request.url, network::URLLoaderCompletionStatus(net::OK),
      response_head, "");

  ASSERT_EQ(SendWebPushMessageResult::kSuccessful, result);
  ASSERT_EQ("message_id", message_id);
}

struct WebPushUrgencyTestData {
  const WebPushMessage::Urgency urgency;
  const std::string expected_header;
} kWebPushUrgencyTestData[] = {
    {WebPushMessage::Urgency::kVeryLow, "very-low"},
    {WebPushMessage::Urgency::kLow, "low"},
    {WebPushMessage::Urgency::kNormal, "normal"},
    {WebPushMessage::Urgency::kHigh, "high"},
};

class WebPushUrgencyTest
    : public WebPushSenderTest,
      public testing::WithParamInterface<WebPushUrgencyTestData> {};

TEST_P(WebPushUrgencyTest, SetUrgencyTest) {
  std::string private_key_info;
  ASSERT_TRUE(base::Base64Decode(kPrivateKey, &private_key_info));
  std::unique_ptr<crypto::ECPrivateKey> private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(std::vector<uint8_t>(
          private_key_info.begin(), private_key_info.end()));
  base::Optional<std::string> message_id;
  std::string urgency;

  WebPushMessage message = CreateMessage();
  message.urgency = GetParam().urgency;

  sender()->SendMessage("token", private_key.get(), std::move(message),
                        base::DoNothing());
  ASSERT_EQ(loader().NumPending(), 1);
  net::HttpRequestHeaders headers =
      loader().GetPendingRequest(0)->request.headers;

  ASSERT_TRUE(headers.GetHeader("Urgency", &urgency));
  ASSERT_EQ(GetParam().expected_header, urgency);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebPushUrgencyTest,
    testing::ValuesIn(kWebPushUrgencyTestData));

struct WebPushHttpStatusTestData {
  const net::Error error_code;
  const net::HttpStatusCode http_status;
  const SendWebPushMessageResult expected_result;
} kWebPushHttpStatusTestData[] = {
    {net::ERR_INSUFFICIENT_RESOURCES, net::HTTP_OK,
     SendWebPushMessageResult::kVapidKeyInvalid},
    {net::ERR_ABORTED, net::HTTP_OK, SendWebPushMessageResult::kNetworkError},
    {net::OK, net::HTTP_OK,
     SendWebPushMessageResult::kParseResponseFailed},  // As no header is set
    {net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
     SendWebPushMessageResult::kServerError},
    {net::OK, net::HTTP_NOT_FOUND, SendWebPushMessageResult::kDeviceGone},
    {net::OK, net::HTTP_GONE, SendWebPushMessageResult::kDeviceGone},
    {net::OK, net::HTTP_BAD_REQUEST,
     SendWebPushMessageResult::kPayloadTooLarge},
    {net::OK, net::HTTP_REQUEST_ENTITY_TOO_LARGE,
     SendWebPushMessageResult::kPayloadTooLarge},
};

class WebPushHttpStatusTest
    : public WebPushSenderTest,
      public testing::WithParamInterface<WebPushHttpStatusTestData> {};

TEST_P(WebPushHttpStatusTest, HttpStatusTest) {
  std::string private_key_info;
  ASSERT_TRUE(base::Base64Decode(kPrivateKey, &private_key_info));
  std::unique_ptr<crypto::ECPrivateKey> private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(std::vector<uint8_t>(
          private_key_info.begin(), private_key_info.end()));
  ASSERT_TRUE(private_key);

  SendWebPushMessageResult result;
  base::Optional<std::string> message_id;
  sender()->SendMessage(
      "fcm_token", private_key.get(), CreateMessage(),
      base::BindOnce(&WebPushSenderTest::OnMessageSent, base::Unretained(this),
                     &result, &message_id));

  ASSERT_EQ(loader().NumPending(), 1);
  loader().SimulateResponseForPendingRequest(
      loader().GetPendingRequest(0)->request.url,
      network::URLLoaderCompletionStatus(GetParam().error_code),
      network::CreateResourceResponseHead(GetParam().http_status), "");

  ASSERT_EQ(GetParam().expected_result, result);
  ASSERT_FALSE(message_id);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebPushHttpStatusTest,
    testing::ValuesIn(kWebPushHttpStatusTestData));

}  // namespace gcm
