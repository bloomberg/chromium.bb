// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_handler.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace cast_channel {

namespace {

constexpr char kTestUserAgentString[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/66.0.3331.0 Safari/537.36";
constexpr char kSourceId[] = "sourceId";
constexpr char kDestinationId[] = "destinationId";

std::unique_ptr<base::Value> GetDictionaryFromCastMessage(
    const CastMessage& message) {
  if (!message.has_payload_utf8())
    return nullptr;

  return base::JSONReader::Read(message.payload_utf8());
}

CastMessageType GetMessageType(const CastMessage& message) {
  std::unique_ptr<base::Value> dict = GetDictionaryFromCastMessage(message);
  if (!dict)
    return CastMessageType::kOther;

  const base::Value* message_type =
      dict->FindKeyOfType("type", base::Value::Type::STRING);
  if (!message_type)
    return CastMessageType::kOther;

  return CastMessageTypeFromString(message_type->GetString());
}

}  // namespace

class CastMessageHandlerTest : public testing::Test {
 public:
  CastMessageHandlerTest()
      : environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        thread_bundle_(content::TestBrowserThreadBundle::PLAIN_MAINLOOP),
        cast_socket_service_(new base::TestSimpleTaskRunner()),
        handler_(&cast_socket_service_,
                 /* connector */ nullptr,
                 "batchId",
                 kTestUserAgentString,
                 "66.0.3331.0",
                 "en-US") {
    ON_CALL(cast_socket_service_, GetSocket(_))
        .WillByDefault(testing::Return(&cast_socket_));
  }

  ~CastMessageHandlerTest() override {}

  void OnMessage(const CastMessage& message) {
    handler_.OnMessage(cast_socket_, message);
  }

  void OnError(ChannelError error) { handler_.OnError(cast_socket_, error); }

  void OnAppAvailability(const std::string& app_id,
                         GetAppAvailabilityResult result) {
    if (run_loop_)
      run_loop_->Quit();
    DoOnAppAvailability(app_id, result);
  }

  MOCK_METHOD2(DoOnAppAvailability,
               void(const std::string& app_id,
                    GetAppAvailabilityResult result));

  void ExpectSessionLaunchResult(LaunchSessionResponse::Result expected_result,
                                 LaunchSessionResponse response) {
    if (run_loop_)
      run_loop_->Quit();
    ++session_launch_response_count_;
    EXPECT_EQ(expected_result, response.result);
    if (response.result == LaunchSessionResponse::Result::kOk)
      EXPECT_TRUE(response.receiver_status);
  }

 protected:
  base::test::ScopedTaskEnvironment environment_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<base::RunLoop> run_loop_;
  MockCastSocketService cast_socket_service_;
  data_decoder::TestingJsonParser::ScopedFactoryOverride parser_override_;
  CastMessageHandler handler_;
  MockCastSocket cast_socket_;
  int session_launch_response_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastMessageHandlerTest);
};

TEST_F(CastMessageHandlerTest, VirtualConnectionCreatedOnlyOnce) {
  CastMessage virtual_connection_request;
  CastMessage app_availability_request1;
  CastMessage app_availability_request2;
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _))
      .WillOnce(SaveArg<0>(&virtual_connection_request))
      .WillOnce(SaveArg<0>(&app_availability_request1))
      .WillOnce(SaveArg<0>(&app_availability_request2));

  handler_.RequestAppAvailability(
      &cast_socket_, "AAAAAAAA",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
  handler_.RequestAppAvailability(
      &cast_socket_, "BBBBBBBB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  EXPECT_EQ(CastMessageType::kConnect,
            GetMessageType(virtual_connection_request));
  EXPECT_EQ(CastMessageType::kGetAppAvailability,
            GetMessageType(app_availability_request1));
  EXPECT_EQ(CastMessageType::kGetAppAvailability,
            GetMessageType(app_availability_request2));
}

TEST_F(CastMessageHandlerTest, RecreateVirtualConnectionAfterError) {
  CastMessage virtual_connection_request;
  CastMessage app_availability_request;
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _))
      .WillOnce(SaveArg<0>(&virtual_connection_request))
      .WillOnce(SaveArg<0>(&app_availability_request));

  handler_.RequestAppAvailability(
      &cast_socket_, "AAAAAAAA",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
  EXPECT_EQ(CastMessageType::kConnect,
            GetMessageType(virtual_connection_request));
  EXPECT_EQ(CastMessageType::kGetAppAvailability,
            GetMessageType(app_availability_request));

  EXPECT_CALL(*this, DoOnAppAvailability("AAAAAAAA",
                                         GetAppAvailabilityResult::kUnknown));
  OnError(ChannelError::TRANSPORT_ERROR);

  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _))
      .WillOnce(SaveArg<0>(&virtual_connection_request))
      .WillOnce(SaveArg<0>(&app_availability_request));

  handler_.RequestAppAvailability(
      &cast_socket_, "BBBBBBBB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  EXPECT_EQ(CastMessageType::kConnect,
            GetMessageType(virtual_connection_request));
  EXPECT_EQ(CastMessageType::kGetAppAvailability,
            GetMessageType(app_availability_request));
  // The callback is invoked with kUnknown before the PendingRequests is
  // destroyed.
  EXPECT_CALL(*this, DoOnAppAvailability("BBBBBBBB",
                                         GetAppAvailabilityResult::kUnknown));
}

TEST_F(CastMessageHandlerTest, RequestAppAvailability) {
  CastMessage virtual_connection_request;
  CastMessage app_availability_request;
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _))
      .WillOnce(SaveArg<0>(&virtual_connection_request))
      .WillOnce(SaveArg<0>(&app_availability_request));

  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  EXPECT_EQ(CastMessageType::kConnect,
            GetMessageType(virtual_connection_request));
  EXPECT_EQ(CastMessageType::kGetAppAvailability,
            GetMessageType(app_availability_request));

  std::unique_ptr<base::Value> dict =
      GetDictionaryFromCastMessage(app_availability_request);
  ASSERT_TRUE(dict);
  const base::Value* request_id_value =
      dict->FindKeyOfType("requestId", base::Value::Type::INTEGER);
  ASSERT_TRUE(request_id_value);
  int request_id = request_id_value->GetInt();
  EXPECT_GT(request_id, 0);

  CastMessage response;
  response.set_namespace_("urn:x-cast:com.google.cast.receiver");
  response.set_source_id("receiver-0");
  response.set_destination_id(handler_.sender_id());
  response.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  response.set_payload_utf8(
      base::StringPrintf("{\"requestId\": %d, \"availability\": {\"ABCDEFAB\": "
                         "\"APP_AVAILABLE\"}}",
                         request_id));

  run_loop_ = std::make_unique<base::RunLoop>();
  EXPECT_CALL(*this, DoOnAppAvailability("ABCDEFAB",
                                         GetAppAvailabilityResult::kAvailable));
  OnMessage(response);
  run_loop_->Run();
}

TEST_F(CastMessageHandlerTest, RequestAppAvailabilityTimesOut) {
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _)).Times(2);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
  EXPECT_CALL(*this, DoOnAppAvailability("ABCDEFAB",
                                         GetAppAvailabilityResult::kUnknown));
  environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
}

TEST_F(CastMessageHandlerTest, AppAvailabilitySentOnlyOnceWhilePending) {
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _)).Times(2);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _)).Times(0);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
}

TEST_F(CastMessageHandlerTest, EnsureConnection) {
  CastMessage virtual_connection_request;
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _))
      .WillOnce(SaveArg<0>(&virtual_connection_request));

  handler_.EnsureConnection(cast_socket_.id(), kSourceId, kDestinationId);
  EXPECT_EQ(CastMessageType::kConnect,
            GetMessageType(virtual_connection_request));

  // No-op because connection is already created the first time.
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _)).Times(0);
  handler_.EnsureConnection(cast_socket_.id(), kSourceId, kDestinationId);
}

TEST_F(CastMessageHandlerTest, CloseConnectionFromReceiver) {
  CastMessage virtual_connection_request;
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _));
  handler_.EnsureConnection(cast_socket_.id(), kSourceId, kDestinationId);

  CastMessage response;
  response.set_namespace_("urn:x-cast:com.google.cast.tp.connection");
  response.set_source_id(kDestinationId);
  response.set_destination_id(kSourceId);
  response.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  response.set_payload_utf8(R"({
      "type": "CLOSE"
  })");
  OnMessage(response);
  // Wait for message to be parsed and handled.
  environment_.RunUntilIdle();

  // Re-open virtual connection should cause message to be sent.
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _));
  handler_.EnsureConnection(cast_socket_.id(), kSourceId, kDestinationId);
}

TEST_F(CastMessageHandlerTest, LaunchSession) {
  CastMessage virtual_connection_request;
  CastMessage launch_session_request;
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _))
      .WillOnce(SaveArg<0>(&virtual_connection_request))
      .WillOnce(SaveArg<0>(&launch_session_request));

  handler_.LaunchSession(
      cast_socket_.id(), "AAAAAAAA", base::TimeDelta::FromSeconds(30),
      base::BindOnce(&CastMessageHandlerTest::ExpectSessionLaunchResult,
                     base::Unretained(this),
                     LaunchSessionResponse::Result::kOk));
  EXPECT_EQ(CastMessageType::kConnect,
            GetMessageType(virtual_connection_request));
  EXPECT_EQ(CastMessageType::kLaunch, GetMessageType(launch_session_request));

  std::unique_ptr<base::Value> dict =
      GetDictionaryFromCastMessage(launch_session_request);
  ASSERT_TRUE(dict);
  const base::Value* request_id_value =
      dict->FindKeyOfType("requestId", base::Value::Type::INTEGER);
  ASSERT_TRUE(request_id_value);
  int request_id = request_id_value->GetInt();
  EXPECT_GT(request_id, 0);

  CastMessage response;
  response.set_namespace_("urn:x-cast:com.google.cast.receiver");
  response.set_source_id("receiver-0");
  response.set_destination_id(handler_.sender_id());
  response.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  response.set_payload_utf8(
      base::StringPrintf("{"
                         "\"type\": \"RECEIVER_STATUS\","
                         "\"requestId\": %d,"
                         "\"status\": {}"
                         "}",
                         request_id));

  run_loop_ = std::make_unique<base::RunLoop>();
  OnMessage(response);
  run_loop_->Run();
  EXPECT_EQ(1, session_launch_response_count_);
}

TEST_F(CastMessageHandlerTest, LaunchSessionTimedOut) {
  CastMessage virtual_connection_request;
  CastMessage launch_session_request;
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _))
      .WillOnce(SaveArg<0>(&virtual_connection_request))
      .WillOnce(SaveArg<0>(&launch_session_request));

  handler_.LaunchSession(
      cast_socket_.id(), "AAAAAAAA", base::TimeDelta::FromSeconds(30),
      base::BindOnce(&CastMessageHandlerTest::ExpectSessionLaunchResult,
                     base::Unretained(this),
                     LaunchSessionResponse::Result::kTimedOut));
  EXPECT_EQ(CastMessageType::kConnect,
            GetMessageType(virtual_connection_request));
  EXPECT_EQ(CastMessageType::kLaunch, GetMessageType(launch_session_request));

  environment_.FastForwardBy(base::TimeDelta::FromSeconds(30));
  EXPECT_EQ(1, session_launch_response_count_);
}

TEST_F(CastMessageHandlerTest, SendAppMessage) {
  CastMessage virtual_connection_request;
  CastMessage app_message;
  EXPECT_CALL(*cast_socket_.mock_transport(), SendMessage(_, _))
      .WillOnce(SaveArg<0>(&virtual_connection_request))
      .WillOnce(SaveArg<0>(&app_message));

  base::Value body(base::Value::Type::DICTIONARY);
  body.SetKey("foo", base::Value("bar"));
  CastMessage message =
      CreateCastMessage("namespace", body, kSourceId, kDestinationId);
  handler_.SendAppMessage(cast_socket_.id(), message);

  EXPECT_EQ(CastMessageType::kConnect,
            GetMessageType(virtual_connection_request));
  EXPECT_EQ(message.payload_utf8(), app_message.payload_utf8());
}

}  // namespace cast_channel
