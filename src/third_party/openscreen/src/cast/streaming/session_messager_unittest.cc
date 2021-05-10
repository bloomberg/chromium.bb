// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/session_messager.h"

#include "cast/streaming/testing/message_pipe.h"
#include "cast/streaming/testing/simple_message_port.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace cast {

using ::testing::ElementsAre;

namespace {

constexpr char kSenderId[] = "sender-12345";
constexpr char kReceiverId[] = "receiver-12345";

// Generally the messages are inlined below, with the exception of the Offer,
// simply because it is massive.
Offer kExampleOffer{
    CastMode::kMirroring,
    false,
    {AudioStream{Stream{0,
                        Stream::Type::kAudioSource,
                        2,
                        RtpPayloadType::kAudioOpus,
                        12344442,
                        std::chrono::milliseconds{2000},
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        false,
                        "",
                        48000},
                 AudioCodec::kOpus, 1400}},
    {VideoStream{Stream{1,
                        Stream::Type::kVideoSource,
                        1,
                        RtpPayloadType::kVideoVp8,
                        12344444,
                        std::chrono::milliseconds{2000},
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        false,
                        "",
                        90000},
                 VideoCodec::kVp8,
                 SimpleFraction{30, 1},
                 3000000,
                 "",
                 "",
                 "",
                 {Resolution{640, 480}},
                 ""}}};

struct SessionMessageStore {
 public:
  SenderSessionMessager::ReplyCallback GetReplyCallback() {
    return [this](ReceiverMessage message) {
      receiver_messages.push_back(std::move(message));
    };
  }

  ReceiverSessionMessager::RequestCallback GetRequestCallback() {
    return [this](SenderMessage message) {
      sender_messages.push_back(std::move(message));
    };
  }

  SessionMessager::ErrorCallback GetErrorCallback() {
    return [this](Error error) { errors.push_back(std::move(error)); };
  }

  std::vector<SenderMessage> sender_messages;
  std::vector<ReceiverMessage> receiver_messages;
  std::vector<Error> errors;
};
}  // namespace

class SessionMessagerTest : public ::testing::Test {
 public:
  SessionMessagerTest()
      : clock_{Clock::now()},
        task_runner_(&clock_),
        message_store_(),
        pipe_(kSenderId, kReceiverId),
        receiver_messager_(pipe_.right(),
                           kReceiverId,
                           message_store_.GetErrorCallback()),
        sender_messager_(pipe_.left(),
                         kSenderId,
                         kReceiverId,
                         message_store_.GetErrorCallback(),
                         &task_runner_)

  {}

  void SetUp() override {
    sender_messager_.SetHandler(ReceiverMessage::Type::kRpc,
                                message_store_.GetReplyCallback());
    receiver_messager_.SetHandler(SenderMessage::Type::kOffer,
                                  message_store_.GetRequestCallback());
    receiver_messager_.SetHandler(SenderMessage::Type::kGetStatus,
                                  message_store_.GetRequestCallback());
    receiver_messager_.SetHandler(SenderMessage::Type::kGetCapabilities,
                                  message_store_.GetRequestCallback());
    receiver_messager_.SetHandler(SenderMessage::Type::kRpc,
                                  message_store_.GetRequestCallback());
  }

 protected:
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  SessionMessageStore message_store_;
  MessagePipe pipe_;
  ReceiverSessionMessager receiver_messager_;
  SenderSessionMessager sender_messager_;

  std::vector<Error> receiver_errors_;
  std::vector<Error> sender_errors_;
};

TEST_F(SessionMessagerTest, RpcMessaging) {
  ASSERT_TRUE(sender_messager_
                  .SendOutboundMessage(SenderMessage{
                      SenderMessage::Type::kRpc, 123, true /* valid */,
                      std::string("all your base are belong to us")})
                  .ok());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
  EXPECT_EQ(SenderMessage::Type::kRpc, message_store_.sender_messages[0].type);
  ASSERT_TRUE(message_store_.sender_messages[0].valid);
  EXPECT_EQ("all your base are belong to us",
            absl::get<std::string>(message_store_.sender_messages[0].body));

  message_store_.sender_messages.clear();
  ASSERT_TRUE(
      receiver_messager_
          .SendMessage(ReceiverMessage{ReceiverMessage::Type::kRpc, 123,
                                       true /* valid */, std::string("nuh uh")})
          .ok());

  ASSERT_TRUE(message_store_.sender_messages.empty());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kRpc,
            message_store_.receiver_messages[0].type);
  EXPECT_TRUE(message_store_.receiver_messages[0].valid);
  EXPECT_EQ("nuh uh",
            absl::get<std::string>(message_store_.receiver_messages[0].body));
}

TEST_F(SessionMessagerTest, StatusMessaging) {
  ASSERT_TRUE(sender_messager_
                  .SendRequest(SenderMessage{SenderMessage::Type::kGetStatus,
                                             3123, true /* valid */},
                               ReceiverMessage::Type::kStatusResponse,
                               message_store_.GetReplyCallback())
                  .ok());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
  EXPECT_EQ(SenderMessage::Type::kGetStatus,
            message_store_.sender_messages[0].type);
  EXPECT_TRUE(message_store_.sender_messages[0].valid);

  message_store_.sender_messages.clear();
  ASSERT_TRUE(
      receiver_messager_
          .SendMessage(ReceiverMessage{
              ReceiverMessage::Type::kStatusResponse, 3123, true /* valid */,
              ReceiverWifiStatus{-5.7, std::vector<int32_t>{1200, 1300, 1250}}})
          .ok());

  ASSERT_TRUE(message_store_.sender_messages.empty());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kStatusResponse,
            message_store_.receiver_messages[0].type);
  EXPECT_TRUE(message_store_.receiver_messages[0].valid);

  const auto& status =
      absl::get<ReceiverWifiStatus>(message_store_.receiver_messages[0].body);
  EXPECT_DOUBLE_EQ(-5.7, status.wifi_snr);
  EXPECT_THAT(status.wifi_speed, ElementsAre(1200, 1300, 1250));
}

TEST_F(SessionMessagerTest, CapabilitiesMessaging) {
  ASSERT_TRUE(
      sender_messager_
          .SendRequest(SenderMessage{SenderMessage::Type::kGetCapabilities,
                                     1337, true /* valid */},
                       ReceiverMessage::Type::kCapabilitiesResponse,
                       message_store_.GetReplyCallback())
          .ok());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
  EXPECT_EQ(SenderMessage::Type::kGetCapabilities,
            message_store_.sender_messages[0].type);
  EXPECT_TRUE(message_store_.sender_messages[0].valid);

  message_store_.sender_messages.clear();
  ASSERT_TRUE(receiver_messager_
                  .SendMessage(ReceiverMessage{
                      ReceiverMessage::Type::kCapabilitiesResponse, 1337,
                      true /* valid */, ReceiverCapability{47, {"ac3", "4k"}}})
                  .ok());

  ASSERT_TRUE(message_store_.sender_messages.empty());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kCapabilitiesResponse,
            message_store_.receiver_messages[0].type);
  EXPECT_TRUE(message_store_.receiver_messages[0].valid);

  const auto& capability =
      absl::get<ReceiverCapability>(message_store_.receiver_messages[0].body);
  EXPECT_EQ(47, capability.remoting_version);
  EXPECT_THAT(capability.media_capabilities, ElementsAre("ac3", "4k"));
}

TEST_F(SessionMessagerTest, OfferAnswerMessaging) {
  ASSERT_TRUE(sender_messager_
                  .SendRequest(SenderMessage{SenderMessage::Type::kOffer, 42,
                                             true /* valid */, kExampleOffer},
                               ReceiverMessage::Type::kAnswer,
                               message_store_.GetReplyCallback())
                  .ok());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
  EXPECT_EQ(SenderMessage::Type::kOffer,
            message_store_.sender_messages[0].type);
  EXPECT_TRUE(message_store_.sender_messages[0].valid);
  message_store_.sender_messages.clear();

  EXPECT_TRUE(receiver_messager_
                  .SendMessage(ReceiverMessage{
                      ReceiverMessage::Type::kAnswer, 41, true /* valid */,
                      Answer{1234, {0, 1}, {12344443, 12344445}}})
                  .ok());
  // A stale answer (for offer 41) should get ignored:
  ASSERT_TRUE(message_store_.sender_messages.empty());
  ASSERT_TRUE(message_store_.receiver_messages.empty());

  ASSERT_TRUE(receiver_messager_
                  .SendMessage(ReceiverMessage{
                      ReceiverMessage::Type::kAnswer, 42, true /* valid */,
                      Answer{1234, {0, 1}, {12344443, 12344445}}})
                  .ok());
  EXPECT_TRUE(message_store_.sender_messages.empty());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kAnswer,
            message_store_.receiver_messages[0].type);
  EXPECT_TRUE(message_store_.receiver_messages[0].valid);

  const auto& answer =
      absl::get<Answer>(message_store_.receiver_messages[0].body);
  EXPECT_EQ(1234, answer.udp_port);

  EXPECT_THAT(answer.send_indexes, ElementsAre(0, 1));
  EXPECT_THAT(answer.ssrcs, ElementsAre(12344443, 12344445));
}

TEST_F(SessionMessagerTest, OfferAndReceiverError) {
  ASSERT_TRUE(sender_messager_
                  .SendRequest(SenderMessage{SenderMessage::Type::kOffer, 42,
                                             true /* valid */, kExampleOffer},
                               ReceiverMessage::Type::kAnswer,
                               message_store_.GetReplyCallback())
                  .ok());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
  EXPECT_EQ(SenderMessage::Type::kOffer,
            message_store_.sender_messages[0].type);
  EXPECT_TRUE(message_store_.sender_messages[0].valid);
  message_store_.sender_messages.clear();

  EXPECT_TRUE(receiver_messager_
                  .SendMessage(ReceiverMessage{
                      ReceiverMessage::Type::kAnswer, 42, false /* valid */,
                      ReceiverError{123, "Something real bad happened"}})
                  .ok());

  EXPECT_TRUE(message_store_.sender_messages.empty());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kAnswer,
            message_store_.receiver_messages[0].type);
  EXPECT_FALSE(message_store_.receiver_messages[0].valid);

  const auto& error =
      absl::get<ReceiverError>(message_store_.receiver_messages[0].body);
  EXPECT_EQ(123, error.code);
  EXPECT_EQ("Something real bad happened", error.description);
}

TEST_F(SessionMessagerTest, UnexpectedMessagesAreIgnored) {
  EXPECT_FALSE(
      receiver_messager_
          .SendMessage(ReceiverMessage{
              ReceiverMessage::Type::kStatusResponse, 3123, true /* valid */,
              ReceiverWifiStatus{-5.7, std::vector<int32_t>{1200, 1300, 1250}}})
          .ok());

  // The message gets dropped and thus won't be in the store.
  EXPECT_TRUE(message_store_.sender_messages.empty());
  EXPECT_TRUE(message_store_.receiver_messages.empty());
}

TEST_F(SessionMessagerTest, UnknownSenderMessageTypesDontGetSent) {
  EXPECT_DEATH(sender_messager_
                   .SendOutboundMessage(SenderMessage{
                       SenderMessage::Type::kUnknown, 123, true /* valid */})
                   .ok(),
               ".*Trying to send an unknown message is a developer error.*");
}

TEST_F(SessionMessagerTest, UnknownReceiverMessageTypesDontGetSent) {
  ASSERT_TRUE(sender_messager_
                  .SendRequest(SenderMessage{SenderMessage::Type::kOffer, 42,
                                             true /* valid */, kExampleOffer},
                               ReceiverMessage::Type::kAnswer,
                               message_store_.GetReplyCallback())
                  .ok());

  EXPECT_DEATH(receiver_messager_
                   .SendMessage(ReceiverMessage{ReceiverMessage::Type::kUnknown,
                                                3123, true /* valid */})
                   .ok(),
               ".*Trying to send an unknown message is a developer error.*");
}

TEST_F(SessionMessagerTest, ReceiverHandlesUnknownMessageType) {
  pipe_.right()->ReceiveMessage(kCastWebrtcNamespace, R"({
    "type": "GET_VIRTUAL_REALITY",
    "seqNum": 31337
  })");
  ASSERT_TRUE(message_store_.errors.empty());
}

TEST_F(SessionMessagerTest, SenderHandlesUnknownMessageType) {
  // The behavior on the sender side is a little more interesting: we
  // test elsewhere that messages with the wrong sequence number are ignored,
  // here if the type is unknown but the message contains a valid sequence
  // number we just treat it as a bad response/same as a timeout.
  ASSERT_TRUE(sender_messager_
                  .SendRequest(SenderMessage{SenderMessage::Type::kOffer, 42,
                                             true /* valid */, kExampleOffer},
                               ReceiverMessage::Type::kAnswer,
                               message_store_.GetReplyCallback())
                  .ok());
  pipe_.left()->ReceiveMessage(kCastWebrtcNamespace, R"({
    "type": "ANSWER_VERSION_2",
    "seqNum": 42
  })");

  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  ASSERT_EQ(ReceiverMessage::Type::kUnknown,
            message_store_.receiver_messages[0].type);
  ASSERT_EQ(false, message_store_.receiver_messages[0].valid);
}

TEST_F(SessionMessagerTest, SenderHandlesMessageMissingSequenceNumber) {
  ASSERT_TRUE(
      sender_messager_
          .SendRequest(SenderMessage{SenderMessage::Type::kGetCapabilities, 42,
                                     true /* valid */},
                       ReceiverMessage::Type::kCapabilitiesResponse,
                       message_store_.GetReplyCallback())
          .ok());
  pipe_.left()->ReceiveMessage(kCastWebrtcNamespace, R"({
    "capabilities": {
      "keySystems": [],
      "mediaCaps": ["video"]
    },
    "result": "ok",
    "type": "CAPABILITIES_RESPONSE"
  })");

  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
}

TEST_F(SessionMessagerTest, ReceiverCannotSendFirst) {
  const Error error = receiver_messager_.SendMessage(ReceiverMessage{
      ReceiverMessage::Type::kStatusResponse, 3123, true /* valid */,
      ReceiverWifiStatus{-5.7, std::vector<int32_t>{1200, 1300, 1250}}});

  EXPECT_EQ(Error::Code::kInitializationFailure, error.code());
}

TEST_F(SessionMessagerTest, ErrorMessageLoggedIfTimeout) {
  ASSERT_TRUE(sender_messager_
                  .SendRequest(SenderMessage{SenderMessage::Type::kGetStatus,
                                             3123, true /* valid */},
                               ReceiverMessage::Type::kStatusResponse,
                               message_store_.GetReplyCallback())
                  .ok());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_TRUE(message_store_.receiver_messages.empty());

  clock_.Advance(std::chrono::seconds(10));
  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(3123, message_store_.receiver_messages[0].sequence_number);
  EXPECT_EQ(ReceiverMessage::Type::kStatusResponse,
            message_store_.receiver_messages[0].type);
  EXPECT_FALSE(message_store_.receiver_messages[0].valid);
}

TEST_F(SessionMessagerTest, ReceiverRejectsMessageFromWrongSender) {
  SimpleMessagePort port(kReceiverId);
  ReceiverSessionMessager messager(&port, kReceiverId,
                                   message_store_.GetErrorCallback());
  messager.SetHandler(SenderMessage::Type::kGetStatus,
                      message_store_.GetRequestCallback());

  // The first message should be accepted since we don't have a set sender_id
  // yet.
  port.ReceiveMessage("sender-31337", kCastWebrtcNamespace, R"({
        "get_status": ["wifiSnr", "wifiSpeed"],
        "seqNum": 820263769,
        "type": "GET_STATUS"
      })");
  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_EQ(1u, message_store_.sender_messages.size());
  message_store_.sender_messages.clear();

  // The second message should just be ignored.
  port.ReceiveMessage("sender-42", kCastWebrtcNamespace, R"({
        "get_status": ["wifiSnr"],
        "seqNum": 1234,
        "type": "GET_STATUS"
      })");
  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_TRUE(message_store_.sender_messages.empty());

  // But the third message should be accepted again since it's from the
  // first sender.
  port.ReceiveMessage("sender-31337", kCastWebrtcNamespace, R"({
        "get_status": ["wifiSnr", "wifiSpeed"],
        "seqNum": 820263769,
        "type": "GET_STATUS"
      })");
  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_EQ(1u, message_store_.sender_messages.size());
}

TEST_F(SessionMessagerTest, SenderRejectsMessageFromWrongSender) {
  SimpleMessagePort port(kReceiverId);
  SenderSessionMessager messager(&port, kSenderId, kReceiverId,
                                 message_store_.GetErrorCallback(),
                                 &task_runner_);

  port.ReceiveMessage("receiver-31337", kCastWebrtcNamespace, R"({
        "seqNum": 12345,
        "sessionId": 735189,
        "type": "RPC",
        "result": "ok",
        "rpc": "SGVsbG8gZnJvbSB0aGUgQ2FzdCBSZWNlaXZlciE="
      })");

  // The message should just be ignored.
  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_TRUE(message_store_.sender_messages.empty());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
}

TEST_F(SessionMessagerTest, ReceiverRejectsMessagesWithoutHandler) {
  SimpleMessagePort port(kReceiverId);
  ReceiverSessionMessager messager(&port, kReceiverId,
                                   message_store_.GetErrorCallback());
  messager.SetHandler(SenderMessage::Type::kGetStatus,
                      message_store_.GetRequestCallback());

  // The first message should be accepted since we don't have a set sender_id
  // yet.
  port.ReceiveMessage("sender-31337", kCastWebrtcNamespace, R"({
        "get_status": ["wifiSnr", "wifiSpeed"],
        "seqNum": 820263769,
        "type": "GET_STATUS"
      })");
  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_EQ(1u, message_store_.sender_messages.size());
  message_store_.sender_messages.clear();

  // The second message should be rejected since it doesn't have a handler.
  port.ReceiveMessage("sender-31337", kCastWebrtcNamespace, R"({
        "seqNum": 820263770,
        "type": "GET_CAPABILITIES"
      })");
  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_TRUE(message_store_.sender_messages.empty());
}

TEST_F(SessionMessagerTest, SenderRejectsMessagesWithoutHandler) {
  SimpleMessagePort port(kReceiverId);
  SenderSessionMessager messager(&port, kSenderId, kReceiverId,
                                 message_store_.GetErrorCallback(),
                                 &task_runner_);

  port.ReceiveMessage(kReceiverId, kCastWebrtcNamespace, R"({
        "seqNum": 12345,
        "sessionId": 735189,
        "type": "RPC",
        "result": "ok",
        "rpc": "SGVsbG8gZnJvbSB0aGUgQ2FzdCBSZWNlaXZlciE="
      })");

  // The message should just be ignored.
  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_TRUE(message_store_.sender_messages.empty());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
}

TEST_F(SessionMessagerTest, UnknownNamespaceMessagesGetDropped) {
  pipe_.right()->ReceiveMessage("urn:x-cast:com.google.cast.virtualreality",
                                R"({
        "seqNum": 12345,
        "sessionId": 735190,
        "type": "RPC",
        "rpc": "SGVsbG8gZnJvbSB0aGUgQ2FzdCBSZWNlaXZlciE="
      })");

  pipe_.left()->ReceiveMessage("urn:x-cast:com.google.cast.virtualreality", R"({
        "seqNum": 12345,
        "sessionId": 735190,
        "type": "RPC",
        "result": "ok",
        "rpc": "SGVsbG8gZnJvbSB0aGUgQ2FzdCBTZW5kZXIh="
      })");

  // The message should just be ignored.
  ASSERT_TRUE(message_store_.errors.empty());
  ASSERT_TRUE(message_store_.sender_messages.empty());
  ASSERT_TRUE(message_store_.receiver_messages.empty());
}

}  // namespace cast
}  // namespace openscreen
