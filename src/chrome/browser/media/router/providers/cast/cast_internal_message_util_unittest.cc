// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"

#include "base/json/json_reader.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

static constexpr char kReceiverIdToken[] = "token";

std::unique_ptr<base::Value> ReceiverStatus() {
  std::string receiver_status_str = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  return base::JSONReader::Read(receiver_status_str);
}

void ExpectNoCastSession(const MediaSinkInternal& sink,
                         const std::string& receiver_status_str,
                         const std::string& reason) {
  auto receiver_status = base::JSONReader::Read(receiver_status_str);
  ASSERT_TRUE(receiver_status);
  auto session = CastSession::From(sink, kReceiverIdToken, *receiver_status);
  EXPECT_FALSE(session) << "Shouldn't have created session because of "
                        << reason;
}

void ExpectJSONMessagesEqual(const std::string& expected_message,
                             const std::string& message) {
  auto expected_message_value = base::JSONReader::Read(expected_message);
  ASSERT_TRUE(expected_message_value);

  auto message_value = base::JSONReader::Read(message);
  ASSERT_TRUE(message_value);

  EXPECT_EQ(*expected_message_value, *message_value);
}

void ExpectInvalidCastInternalMessage(const std::string& message_str,
                                      const std::string& invalid_reason) {
  auto message_value = base::JSONReader::Read(message_str);
  ASSERT_TRUE(message_value);
  EXPECT_FALSE(CastInternalMessage::From(std::move(*message_value)))
      << "message expected to be invlaid: " << invalid_reason;
}

}  // namespace

TEST(CastInternalMessageUtilTest, CastInternalMessageFromAppMessageString) {
  std::string message_str = R"({
    "type": "app_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "namespaceName": "urn:x-cast:com.google.foo",
      "sessionId": "sessionId",
      "message": { "foo": "bar" }
    }
  })";
  auto message_value = base::JSONReader::Read(message_str);
  ASSERT_TRUE(message_value);

  auto message = CastInternalMessage::From(std::move(*message_value));
  ASSERT_TRUE(message);
  EXPECT_EQ(CastInternalMessage::Type::kAppMessage, message->type);
  EXPECT_EQ("12345", message->client_id);
  EXPECT_EQ(999, message->sequence_number);
  EXPECT_EQ("urn:x-cast:com.google.foo", message->app_message_namespace);
  EXPECT_EQ("sessionId", message->app_message_session_id);
  base::Value message_body(base::Value::Type::DICTIONARY);
  message_body.SetKey("foo", base::Value("bar"));
  EXPECT_EQ(message_body, message->app_message_body);
}

TEST(CastInternalMessageUtilTest, CastInternalMessageFromClientConnectString) {
  std::string message_str = R"({
      "type": "client_connect",
      "clientId": "12345",
      "message": {}
    })";
  auto message_value = base::JSONReader::Read(message_str);
  ASSERT_TRUE(message_value);

  auto message = CastInternalMessage::From(std::move(*message_value));
  ASSERT_TRUE(message);
  EXPECT_EQ(CastInternalMessage::Type::kClientConnect, message->type);
  EXPECT_EQ("12345", message->client_id);
  EXPECT_EQ(-1, message->sequence_number);
  EXPECT_TRUE(message->app_message_namespace.empty());
  EXPECT_TRUE(message->app_message_session_id.empty());
  EXPECT_EQ(base::Value(), message->app_message_body);
}

TEST(CastInternalMessageUtilTest, CastInternalMessageFromInvalidStrings) {
  std::string unknown_type = R"({
      "type": "some_unknown_type",
      "clientId": "12345",
      "message": {}
    })";
  ExpectInvalidCastInternalMessage(unknown_type, "unknown_type");

  std::string missing_client_id = R"({
      "type": "client_connect",
      "message": {}
    })";
  ExpectInvalidCastInternalMessage(missing_client_id, "missing client ID");

  std::string missing_message = R"({
      "type": "client_connect",
      "clientId": "12345"
    })";
  ExpectInvalidCastInternalMessage(missing_message, "missing message");

  std::string app_message_missing_namespace = R"({
    "type": "app_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "sessionId": "sessionId",
      "message": { "foo": "bar" }
    }
  })";
  ExpectInvalidCastInternalMessage(app_message_missing_namespace,
                                   "missing namespace");

  std::string app_message_missing_session_id = R"({
    "type": "app_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "namespaceName": "urn:x-cast:com.google.foo",
      "message": { "foo": "bar" }
    }
  })";
  ExpectInvalidCastInternalMessage(app_message_missing_session_id,
                                   "missing session ID");

  std::string app_message_missing_message = R"({
    "type": "app_message",
    "clientId": "12345",
    "sequenceNumber": 999,
    "message": {
      "namespaceName": "urn:x-cast:com.google.foo",
      "sessionId": "sessionId"
    }
  })";
  ExpectInvalidCastInternalMessage(app_message_missing_message,
                                   "missing app message");
}

TEST(CastInternalMessageUtilTest, CastSessionFromReceiverStatusNoStatusText) {
  MediaSinkInternal sink = CreateCastSink(1);
  std::string receiver_status_str = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "transportId":"transportId"
      }]
  })";
  auto receiver_status = base::JSONReader::Read(receiver_status_str);
  ASSERT_TRUE(receiver_status);
  auto session = CastSession::From(sink, kReceiverIdToken, *receiver_status);
  ASSERT_TRUE(session);
  EXPECT_EQ("sessionId", session->session_id);
  EXPECT_EQ("ABCDEFGH", session->app_id);
  EXPECT_EQ("transportId", session->transport_id);
  base::flat_set<std::string> message_namespaces = {
      "urn:x-cast:com.google.cast.media", "urn:x-cast:com.google.foo"};
  EXPECT_EQ(message_namespaces, session->message_namespaces);
  EXPECT_TRUE(session->value.is_dict());
  EXPECT_EQ("App display name", CastSession::GetRouteDescription(*session));
}

TEST(CastInternalMessageUtilTest, CastSessionFromInvalidReceiverStatuses) {
  MediaSinkInternal sink = CreateCastSink(1);
  std::string missing_app_id = R"({
      "applications": [{
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  ExpectNoCastSession(sink, missing_app_id, "missing app id");

  std::string missing_display_name = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  ExpectNoCastSession(sink, missing_display_name, "missing display name");

  std::string missing_namespaces = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [],
        "sessionId": "sessionId",
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  ExpectNoCastSession(sink, missing_namespaces, "missing namespaces");

  std::string missing_session_id = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "statusText":"App status",
        "transportId":"transportId"
      }]
  })";
  ExpectNoCastSession(sink, missing_session_id, "missing session id");

  std::string missing_transport_id = R"({
      "applications": [{
        "appId": "ABCDEFGH",
        "displayName": "App display name",
        "namespaces": [
          {"name": "urn:x-cast:com.google.cast.media"},
          {"name": "urn:x-cast:com.google.foo"}
        ],
        "sessionId": "sessionId",
        "statusText":"App status"
      }]
  })";
  ExpectNoCastSession(sink, missing_transport_id, "missing transport id");
}

TEST(CastInternalMessageUtilTest, CreateReceiverActionCastMessage) {
  std::string client_id = "clientId";
  MediaSinkInternal sink = CreateCastSink(1);
  std::string expected_message = R"({
     "clientId": "clientId",
     "message": {
        "action": "cast",
        "receiver": {
           "capabilities": [ "video_out", "audio_out" ],
           "displayStatus": null,
           "friendlyName": "friendly name 1",
           "isActiveInput": null,
           "label": "yYH_HCL9CKJFmvKJ9m3Une2cS8s",
           "receiverType": "cast",
           "volume": null
        }
     },
     "sequenceNumber": -1,
     "timeoutMillis": 0,
     "type": "receiver_action"
  })";

  auto message =
      CreateReceiverActionCastMessage(client_id, sink, kReceiverIdToken);
  ExpectJSONMessagesEqual(expected_message, message->get_message());
}

TEST(CastInternalMessageUtilTest, CreateReceiverActionStopMessage) {
  std::string client_id = "clientId";
  MediaSinkInternal sink = CreateCastSink(1);
  std::string expected_message = R"({
     "clientId": "clientId",
     "message": {
        "action": "stop",
        "receiver": {
           "capabilities": [ "video_out", "audio_out" ],
           "displayStatus": null,
           "friendlyName": "friendly name 1",
           "isActiveInput": null,
           "label": "yYH_HCL9CKJFmvKJ9m3Une2cS8s",
           "receiverType": "cast",
           "volume": null
        }
     },
     "sequenceNumber": -1,
     "timeoutMillis": 0,
     "type": "receiver_action"
  })";

  auto message =
      CreateReceiverActionStopMessage(client_id, sink, kReceiverIdToken);
  ExpectJSONMessagesEqual(expected_message, message->get_message());
}

TEST(CastInternalMessageUtilTest, CreateNewSessionMessage) {
  MediaSinkInternal sink = CreateCastSink(1);
  std::string client_id = "clientId";
  auto receiver_status = ReceiverStatus();
  ASSERT_TRUE(receiver_status);
  auto session = CastSession::From(sink, kReceiverIdToken, *receiver_status);
  ASSERT_TRUE(session);

  std::string expected_message = R"({
   "clientId": "clientId",
   "message": {
      "appId": "ABCDEFGH",
      "appImages": [  ],
      "displayName": "App display name",
      "namespaces": [ {
         "name": "urn:x-cast:com.google.cast.media"
      }, {
         "name": "urn:x-cast:com.google.foo"
      } ],
      "receiver": {
         "capabilities": [ "video_out", "audio_out" ],
         "displayStatus": null,
         "friendlyName": "friendly name 1",
         "isActiveInput": null,
         "label": "yYH_HCL9CKJFmvKJ9m3Une2cS8s",
         "receiverType": "cast",
         "volume": null
      },
      "senderApps": [  ],
      "sessionId": "sessionId",
      "statusText": "App status",
      "transportId": "transportId"
   },
   "sequenceNumber": -1,
   "timeoutMillis": 0,
   "type": "new_session"
  })";

  auto message = CreateNewSessionMessage(*session, client_id);
  ExpectJSONMessagesEqual(expected_message, message->get_message());
}

TEST(CastInternalMessageUtilTest, CreateAppMessageAck) {
  std::string client_id = "clientId";
  int sequence_number = 12345;

  std::string expected_message = R"({
   "clientId": "clientId",
   "message": null,
   "sequenceNumber": 12345,
   "timeoutMillis": 0,
   "type": "app_message"
  })";

  auto message = CreateAppMessageAck(client_id, sequence_number);
  ExpectJSONMessagesEqual(expected_message, message->get_message());
}

TEST(CastInternalMessageUtilTest, CreateAppMessage) {
  std::string session_id = "sessionId";
  std::string client_id = "clientId";
  base::Value message_body(base::Value::Type::DICTIONARY);
  message_body.SetKey("foo", base::Value("bar"));
  cast_channel::CastMessage cast_message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", message_body, "sourceId", "destinationId");

  std::string expected_message = R"({
   "clientId": "clientId",
   "message": {
      "message": "{\"foo\":\"bar\"}",
      "namespaceName": "urn:x-cast:com.google.foo",
      "sessionId": "sessionId"
   },
   "sequenceNumber": -1,
   "timeoutMillis": 0,
   "type": "app_message"
  })";

  auto message = CreateAppMessage(session_id, client_id, cast_message);
  ExpectJSONMessagesEqual(expected_message, message->get_message());
}

}  // namespace media_router
