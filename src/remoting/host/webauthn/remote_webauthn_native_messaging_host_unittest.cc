// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/native_messaging/native_messaging_constants.h"
#include "remoting/host/webauthn/remote_webauthn_constants.h"
#include "remoting/host/webauthn/remote_webauthn_native_messaging_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::Return;

using IsUvpaaCallback =
    mojom::WebAuthnProxy::IsUserVerifyingPlatformAuthenticatorAvailableCallback;

base::Value CreateRequestMessage(const std::string& message_type,
                                 int message_id = 1) {
  base::Value request(base::Value::Type::DICTIONARY);
  request.SetStringKey(kMessageType, message_type);
  request.SetIntKey(kMessageId, message_id);
  return request;
}

void VerifyResponseMessage(const base::Value& response,
                           const std::string& request_message_type,
                           int message_id = 1) {
  ASSERT_EQ(*response.FindStringKey(kMessageType),
            request_message_type + "Response");
  ASSERT_EQ(*response.FindIntKey(kMessageId), message_id);
}

class MockWebAuthnProxy : public mojom::WebAuthnProxy {
 public:
  MOCK_METHOD(void,
              IsUserVerifyingPlatformAuthenticatorAvailable,
              (IsUserVerifyingPlatformAuthenticatorAvailableCallback),
              (override));
  MOCK_METHOD(void, Create, (const std::string&, CreateCallback), (override));
};

}  // namespace

class RemoteWebAuthnNativeMessagingHostTest
    : public testing::Test,
      public extensions::NativeMessageHost::Client {
 public:
  RemoteWebAuthnNativeMessagingHostTest();
  ~RemoteWebAuthnNativeMessagingHostTest() override;

  void SetUp() override;

  // extensions::NativeMessageHost::Client implementation.
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

 protected:
  // nullptr will be returned for the GetSessionServices() call if
  // |should_return_valid_services| is false.
  testing::Expectation ExpectGetSessionServices(
      bool should_return_valid_services = true);

  // Receiver will be immediately discarded if |should_bind_receiver| is false.
  testing::Expectation ExpectBindWebAuthnProxy(
      bool should_bind_receiver = true);

  // Sends a message to the native messaging host.
  void SendMessage(const base::Value& message);

  // Blocks until a new message is received, then returns the message.
  const base::Value& ReadMessage();

  MockWebAuthnProxy webauthn_proxy_;
  MockChromotingHostServicesProvider* api_provider_;
  MockChromotingSessionServices api_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<RemoteWebAuthnNativeMessagingHost> host_;
  std::unique_ptr<base::RunLoop> response_run_loop_;
  mojo::Receiver<mojom::WebAuthnProxy> webauthn_proxy_receiver_{
      &webauthn_proxy_};
  base::Value latest_message_;
};

RemoteWebAuthnNativeMessagingHostTest::RemoteWebAuthnNativeMessagingHostTest() {
  auto api_provider = std::make_unique<MockChromotingHostServicesProvider>();
  api_provider_ = api_provider.get();
  host_ = base::WrapUnique(new RemoteWebAuthnNativeMessagingHost(
      std::move(api_provider), task_environment_.GetMainThreadTaskRunner()));
  response_run_loop_ = std::make_unique<base::RunLoop>();
}

RemoteWebAuthnNativeMessagingHostTest::
    ~RemoteWebAuthnNativeMessagingHostTest() = default;

void RemoteWebAuthnNativeMessagingHostTest::SetUp() {
  host_->Start(/* client= */ this);
}

void RemoteWebAuthnNativeMessagingHostTest::PostMessageFromNativeHost(
    const std::string& message) {
  response_run_loop_->Quit();
  auto message_json = base::JSONReader::Read(message);
  ASSERT_TRUE(message_json.has_value());
  latest_message_ = std::move(*message_json);
}

void RemoteWebAuthnNativeMessagingHostTest::CloseChannel(
    const std::string& error_message) {
  NOTREACHED();
}

testing::Expectation
RemoteWebAuthnNativeMessagingHostTest::ExpectGetSessionServices(
    bool should_return_valid_services) {
  return EXPECT_CALL(*api_provider_, GetSessionServices())
      .WillOnce(Return(should_return_valid_services ? &api_ : nullptr));
}

testing::Expectation
RemoteWebAuthnNativeMessagingHostTest::ExpectBindWebAuthnProxy(
    bool should_bind_receiver) {
  if (should_bind_receiver) {
    return EXPECT_CALL(api_, BindWebAuthnProxy(_))
        .WillOnce(
            [&](mojo::PendingReceiver<mojom::WebAuthnProxy> pending_receiver) {
              webauthn_proxy_receiver_.Bind(std::move(pending_receiver));
            });
  }
  // Receiver is immediately discarded.
  return EXPECT_CALL(api_, BindWebAuthnProxy(_)).WillOnce(Return());
}

void RemoteWebAuthnNativeMessagingHostTest::SendMessage(
    const base::Value& message) {
  std::string serialized_message;
  ASSERT_TRUE(base::JSONWriter::Write(message, &serialized_message));
  host_->OnMessage(serialized_message);
}

const base::Value& RemoteWebAuthnNativeMessagingHostTest::ReadMessage() {
  response_run_loop_->Run();
  response_run_loop_ = std::make_unique<base::RunLoop>();
  return latest_message_;
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, HelloRequest) {
  SendMessage(CreateRequestMessage(kHelloMessage));

  const base::Value& response = ReadMessage();
  VerifyResponseMessage(response, kHelloMessage);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       GetRemoteState_FailedToGetSessionServices_NotRemoted) {
  ExpectGetSessionServices(false);
  SendMessage(CreateRequestMessage(kGetRemoteStateMessageType));

  const base::Value& response = ReadMessage();
  VerifyResponseMessage(response, kGetRemoteStateMessageType);
  ASSERT_EQ(*response.FindBoolKey(kGetRemoteStateResponseIsRemotedKey), false);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       GetRemoteState_FailedToBindWebAuthnProxy_NotRemoted) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy(false);
  SendMessage(CreateRequestMessage(kGetRemoteStateMessageType));

  const base::Value& response = ReadMessage();
  VerifyResponseMessage(response, kGetRemoteStateMessageType);
  ASSERT_EQ(*response.FindBoolKey(kGetRemoteStateResponseIsRemotedKey), false);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, GetRemoteState_Remoted) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  SendMessage(CreateRequestMessage(kGetRemoteStateMessageType));

  const base::Value& response = ReadMessage();
  VerifyResponseMessage(response, kGetRemoteStateMessageType);
  ASSERT_EQ(*response.FindBoolKey(kGetRemoteStateResponseIsRemotedKey), true);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, IsUvpaa) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  EXPECT_CALL(webauthn_proxy_, IsUserVerifyingPlatformAuthenticatorAvailable(_))
      .WillOnce(base::test::RunOnceCallback<0>(true));
  SendMessage(CreateRequestMessage(kIsUvpaaMessageType));

  const base::Value& response = ReadMessage();
  VerifyResponseMessage(response, kIsUvpaaMessageType);
  ASSERT_EQ(*response.FindBoolKey(kIsUvpaaResponseIsAvailableKey), true);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, ParallelIsUvpaaRequests) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  IsUvpaaCallback cb_1;
  IsUvpaaCallback cb_2;
  base::RunLoop both_requests_sent_run_loop;
  EXPECT_CALL(webauthn_proxy_, IsUserVerifyingPlatformAuthenticatorAvailable(_))
      .WillOnce([&](IsUvpaaCallback cb) { cb_1 = std::move(cb); })
      .WillOnce([&](IsUvpaaCallback cb) {
        cb_2 = std::move(cb);
        both_requests_sent_run_loop.Quit();
      });

  SendMessage(CreateRequestMessage(kIsUvpaaMessageType, 1));
  SendMessage(CreateRequestMessage(kIsUvpaaMessageType, 2));
  both_requests_sent_run_loop.Run();
  std::move(cb_2).Run(false);
  base::Value response_2 = ReadMessage().Clone();
  std::move(cb_1).Run(true);
  base::Value response_1 = ReadMessage().Clone();

  VerifyResponseMessage(response_1, kIsUvpaaMessageType, 1);
  VerifyResponseMessage(response_2, kIsUvpaaMessageType, 2);
  ASSERT_EQ(*response_1.FindBoolKey(kIsUvpaaResponseIsAvailableKey), true);
  ASSERT_EQ(*response_2.FindBoolKey(kIsUvpaaResponseIsAvailableKey), false);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Create_MalformedRequest_Error) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();

  // Request message missing |requestData| field.
  SendMessage(CreateRequestMessage(kCreateMessageType));

  const base::Value& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  ASSERT_EQ(response.FindStringKey(kCreateResponseDataKey), nullptr);
  ASSERT_NE(response.FindStringKey(kCreateResponseErrorNameKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       Create_IpcConnectionFailed_Error) {
  ExpectGetSessionServices(false);
  auto request = CreateRequestMessage(kCreateMessageType);
  request.SetStringKey(kCreateRequestDataKey, "dummy");
  SendMessage(std::move(request));

  const base::Value& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  ASSERT_EQ(response.FindStringKey(kCreateResponseDataKey), nullptr);
  ASSERT_NE(response.FindStringKey(kCreateResponseErrorNameKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Create_EmptyResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  EXPECT_CALL(webauthn_proxy_, Create("dummy", _))
      .WillOnce(base::test::RunOnceCallback<1>(nullptr));
  auto request = CreateRequestMessage(kCreateMessageType);
  request.SetStringKey(kCreateRequestDataKey, "dummy");
  SendMessage(std::move(request));

  const base::Value& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  ASSERT_EQ(response.FindStringKey(kCreateResponseDataKey), nullptr);
  ASSERT_EQ(response.FindStringKey(kCreateResponseErrorNameKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Create_ErrorResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  auto mojo_response =
      mojom::WebAuthnCreateResponse::NewErrorName("NotSupportedError");
  EXPECT_CALL(webauthn_proxy_, Create("dummy", _))
      .WillOnce(base::test::RunOnceCallback<1>(std::move(mojo_response)));
  auto request = CreateRequestMessage(kCreateMessageType);
  request.SetStringKey(kCreateRequestDataKey, "dummy");
  SendMessage(std::move(request));

  const base::Value& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  ASSERT_EQ(response.FindStringKey(kCreateResponseDataKey), nullptr);
  ASSERT_EQ(*response.FindStringKey(kCreateResponseErrorNameKey),
            "NotSupportedError");
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Create_DataResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  auto mojo_response =
      mojom::WebAuthnCreateResponse::NewResponseData("dummy response");
  EXPECT_CALL(webauthn_proxy_, Create("dummy", _))
      .WillOnce(base::test::RunOnceCallback<1>(std::move(mojo_response)));
  auto request = CreateRequestMessage(kCreateMessageType);
  request.SetStringKey(kCreateRequestDataKey, "dummy");
  SendMessage(std::move(request));

  const base::Value& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  ASSERT_EQ(*response.FindStringKey(kCreateResponseDataKey), "dummy response");
  ASSERT_EQ(response.FindStringKey(kCreateResponseErrorNameKey), nullptr);
}

}  // namespace remoting
