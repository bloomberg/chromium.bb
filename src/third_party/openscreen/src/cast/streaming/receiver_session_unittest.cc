// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include <utility>

#include "cast/streaming/mock_environment.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/ip_address.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace openscreen {
namespace cast {

namespace {

constexpr char kValidOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "receiverGetStatus": true,
    "supportedStreams": [
      {
        "index": 31337,
        "type": "video_source",
        "codecName": "vp9",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088743,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "bbf109bf84513b456b13a184453b66ce",
        "aesIvMask": "edaf9e4536e2b66191f560d9c04b2a69",
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      },
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      },
      {
        "index": 1337,
        "type": "audio_source",
        "codecName": "opus",
        "rtpProfile": "cast",
        "rtpPayloadType": 97,
        "ssrc": 19088747,
        "bitRate": 124000,
        "timeBase": "1/48000",
        "channels": 2,
        "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
        "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1"
      }
    ]
  }
})";

constexpr char kNoAudioOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "receiverGetStatus": true,
    "supportedStreams": [
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      }
    ]
  }
})";

constexpr char kNoVideoOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "receiverGetStatus": true,
    "supportedStreams": [
      {
        "index": 1337,
        "type": "audio_source",
        "codecName": "opus",
        "rtpProfile": "cast",
        "rtpPayloadType": 97,
        "ssrc": 19088747,
        "bitRate": 124000,
        "timeBase": "1/48000",
        "channels": 2,
        "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
        "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1"
      }
    ]
  }
})";

constexpr char kNoAudioOrVideoOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "receiverGetStatus": true,
    "supportedStreams": []
  }
})";

constexpr char kInvalidJsonOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,,,
  "offer":
    "castMode": "mirroring",
    "receiverGetStatus": true,
    "supportedStreams": [
  }
})";

class SimpleMessagePort : public MessagePort {
 public:
  ~SimpleMessagePort() override {}
  void SetClient(MessagePort::Client* client) override { client_ = client; }

  void ReceiveMessage(absl::string_view message) {
    ASSERT_NE(client_, nullptr);
    client_->OnMessage("sender-id", "namespace", message);
  }

  void ReceiveError(Error error) {
    ASSERT_NE(client_, nullptr);
    client_->OnError(error);
  }

  void PostMessage(absl::string_view sender_id,
                   absl::string_view message_namespace,
                   absl::string_view message) override {
    posted_messages_.emplace_back(std::move(message));
  }

  MessagePort::Client* client() const { return client_; }
  const std::vector<std::string> posted_messages() const {
    return posted_messages_;
  }

 private:
  MessagePort::Client* client_ = nullptr;
  std::vector<std::string> posted_messages_;
};

class FakeClient : public ReceiverSession::Client {
 public:
  MOCK_METHOD(void,
              OnNegotiated,
              (const ReceiverSession*, ReceiverSession::ConfiguredReceivers),
              (override));
  MOCK_METHOD(void,
              OnConfiguredReceiversDestroyed,
              (const ReceiverSession*),
              (override));
  MOCK_METHOD(void, OnError, (const ReceiverSession*, Error error), (override));
};

void ExpectIsErrorAnswerMessage(const ErrorOr<Json::Value>& message_or_error) {
  EXPECT_TRUE(message_or_error.is_value());
  const Json::Value message = std::move(message_or_error.value());
  EXPECT_TRUE(message["answer"].isNull());
  EXPECT_EQ("error", message["result"].asString());
  EXPECT_EQ(1337, message["seqNum"].asInt());
  EXPECT_EQ("ANSWER", message["type"].asString());

  const Json::Value& error = message["error"];
  EXPECT_TRUE(error.isObject());
  EXPECT_GT(error["code"].asInt(), 0);
  EXPECT_EQ("", error["description"].asString());
}

}  // namespace

class ReceiverSessionTest : public ::testing::Test {
 public:
  ReceiverSessionTest() : clock_(Clock::time_point{}), task_runner_(&clock_) {}

  std::unique_ptr<MockEnvironment> MakeEnvironment() {
    auto environment = std::make_unique<NiceMock<MockEnvironment>>(
        &FakeClock::now, &task_runner_);
    ON_CALL(*environment, GetBoundLocalEndpoint())
        .WillByDefault(
            Return(IPEndpoint{IPAddress::Parse("127.0.0.1").value(), 12345}));
    return environment;
  }

 private:
  FakeClock clock_;
  FakeTaskRunner task_runner_;
};

TEST_F(ReceiverSessionTest, RegistersSelfOnMessagePump) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  // This should be safe, since the message_port location should not move
  // just because of being moved into the ReceiverSession.
  StrictMock<FakeClient> client;

  auto environment = MakeEnvironment();
  auto session = std::make_unique<ReceiverSession>(
      &client, environment.get(), message_port.get(),
      ReceiverSession::Preferences{});
  EXPECT_EQ(message_port->client(), session.get());
}

TEST_F(ReceiverSessionTest, CanNegotiateWithDefaultPreferences) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  StrictMock<FakeClient> client;
  auto environment = MakeEnvironment();
  ReceiverSession session(&client, environment.get(), message_port.get(),
                          ReceiverSession::Preferences{});

  EXPECT_CALL(client, OnNegotiated(&session, _))
      .WillOnce([](const ReceiverSession* session,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.audio);
        EXPECT_EQ(cr.audio.value().receiver_config.sender_ssrc, 19088747u);
        EXPECT_EQ(cr.audio.value().receiver_config.receiver_ssrc, 19088748u);
        EXPECT_EQ(cr.audio.value().receiver_config.channels, 2);
        EXPECT_EQ(cr.audio.value().receiver_config.rtp_timebase, 48000);

        // We should have chosen opus
        EXPECT_EQ(cr.audio.value().selected_stream.stream.index, 1337);
        EXPECT_EQ(cr.audio.value().selected_stream.stream.type,
                  Stream::Type::kAudioSource);
        EXPECT_EQ(cr.audio.value().selected_stream.stream.codec_name, "opus");
        EXPECT_EQ(cr.audio.value().selected_stream.stream.channels, 2);

        EXPECT_TRUE(cr.video);
        EXPECT_EQ(cr.video.value().receiver_config.sender_ssrc, 19088745u);
        EXPECT_EQ(cr.video.value().receiver_config.receiver_ssrc, 19088746u);
        EXPECT_EQ(cr.video.value().receiver_config.channels, 1);
        EXPECT_EQ(cr.video.value().receiver_config.rtp_timebase, 90000);

        // We should have chosen vp8
        EXPECT_EQ(cr.video.value().selected_stream.stream.index, 31338);
        EXPECT_EQ(cr.video.value().selected_stream.stream.type,
                  Stream::Type::kVideoSource);
        EXPECT_EQ(cr.video.value().selected_stream.stream.codec_name, "vp8");
        EXPECT_EQ(cr.video.value().selected_stream.stream.channels, 1);
      });
  EXPECT_CALL(client, OnConfiguredReceiversDestroyed(&session)).Times(1);

  message_port->ReceiveMessage(kValidOfferMessage);

  const auto& messages = message_port->posted_messages();
  ASSERT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  EXPECT_TRUE(message_body.is_value());
  const Json::Value answer = std::move(message_body.value());

  EXPECT_EQ("ANSWER", answer["type"].asString());
  EXPECT_EQ(1337, answer["seqNum"].asInt());
  EXPECT_EQ("ok", answer["result"].asString());

  const Json::Value& answer_body = answer["answer"];
  EXPECT_TRUE(answer_body.isObject());

  // Spot check the answer body fields. We have more in depth testing
  // of answer behavior in answer_messages_unittest, but here we can
  // ensure that the ReceiverSession properly configured the answer.
  EXPECT_EQ("mirroring", answer_body["castMode"].asString());
  EXPECT_EQ(1337, answer_body["sendIndexes"][0].asInt());
  EXPECT_EQ(31338, answer_body["sendIndexes"][1].asInt());
  EXPECT_LT(0, answer_body["udpPort"].asInt());
  EXPECT_GT(65535, answer_body["udpPort"].asInt());

  // Get status should always be false, as we have no plans to implement it.
  EXPECT_EQ(false, answer_body["receiverGetStatus"].asBool());

  // Constraints and display should not be present with no preferences.
  EXPECT_TRUE(answer_body["constraints"].isNull());
  EXPECT_TRUE(answer_body["display"].isNull());
}

TEST_F(ReceiverSessionTest, CanNegotiateWithCustomCodecPreferences) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  StrictMock<FakeClient> client;
  auto environment = MakeEnvironment();
  ReceiverSession session(
      &client, environment.get(), message_port.get(),
      ReceiverSession::Preferences{{ReceiverSession::VideoCodec::kVp9},
                                   {ReceiverSession::AudioCodec::kOpus}});

  EXPECT_CALL(client, OnNegotiated(&session, _))
      .WillOnce([](const ReceiverSession* session,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.audio);
        EXPECT_EQ(cr.audio.value().receiver_config.sender_ssrc, 19088747u);
        EXPECT_EQ(cr.audio.value().receiver_config.receiver_ssrc, 19088748u);
        EXPECT_EQ(cr.audio.value().receiver_config.channels, 2);
        EXPECT_EQ(cr.audio.value().receiver_config.rtp_timebase, 48000);

        EXPECT_TRUE(cr.video);
        // We should have chosen vp9
        EXPECT_EQ(cr.video.value().receiver_config.sender_ssrc, 19088743u);
        EXPECT_EQ(cr.video.value().receiver_config.receiver_ssrc, 19088744u);
        EXPECT_EQ(cr.video.value().receiver_config.channels, 1);
        EXPECT_EQ(cr.video.value().receiver_config.rtp_timebase, 90000);
      });
  EXPECT_CALL(client, OnConfiguredReceiversDestroyed(&session)).Times(1);
  message_port->ReceiveMessage(kValidOfferMessage);
}

TEST_F(ReceiverSessionTest, CanNegotiateWithCustomConstraints) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  StrictMock<FakeClient> client;

  auto constraints = std::unique_ptr<Constraints>{new Constraints{
      AudioConstraints{1, 2, 3, 4},
      VideoConstraints{3.14159, Dimensions{320, 240, SimpleFraction{24, 1}},
                       Dimensions{1920, 1080, SimpleFraction{144, 1}}, 3000,
                       90000000, std::chrono::milliseconds{1000}}}};

  auto display = std::unique_ptr<DisplayDescription>{new DisplayDescription{
      Dimensions{640, 480, SimpleFraction{60, 1}}, AspectRatio{16, 9},
      AspectRatioConstraint::kFixed}};

  auto environment = MakeEnvironment();
  ReceiverSession session(
      &client, environment.get(), message_port.get(),
      ReceiverSession::Preferences{{ReceiverSession::VideoCodec::kVp9},
                                   {ReceiverSession::AudioCodec::kOpus},
                                   std::move(constraints),
                                   std::move(display)});

  EXPECT_CALL(client, OnNegotiated(&session, _)).Times(1);
  EXPECT_CALL(client, OnConfiguredReceiversDestroyed(&session)).Times(1);
  message_port->ReceiveMessage(kValidOfferMessage);

  const auto& messages = message_port->posted_messages();
  EXPECT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const Json::Value answer = std::move(message_body.value());

  const Json::Value& answer_body = answer["answer"];
  ASSERT_TRUE(answer_body.isObject());

  // Constraints and display should be valid with valid preferences.
  ASSERT_FALSE(answer_body["constraints"].isNull());
  ASSERT_FALSE(answer_body["display"].isNull());

  const Json::Value& display_json = answer_body["display"];
  EXPECT_EQ("16:9", display_json["aspectRatio"].asString());
  EXPECT_EQ("60", display_json["dimensions"]["frameRate"].asString());
  EXPECT_EQ(640, display_json["dimensions"]["width"].asInt());
  EXPECT_EQ(480, display_json["dimensions"]["height"].asInt());
  EXPECT_EQ("sender", display_json["scaling"].asString());

  const Json::Value& constraints_json = answer_body["constraints"];
  ASSERT_TRUE(constraints_json.isObject());

  const Json::Value& audio = constraints_json["audio"];
  ASSERT_TRUE(audio.isObject());
  EXPECT_EQ(4, audio["maxBitRate"].asInt());
  EXPECT_EQ(2, audio["maxChannels"].asInt());
  EXPECT_EQ(0, audio["maxDelay"].asInt());
  EXPECT_EQ(1, audio["maxSampleRate"].asInt());
  EXPECT_EQ(3, audio["minBitRate"].asInt());

  const Json::Value& video = constraints_json["video"];
  ASSERT_TRUE(video.isObject());
  EXPECT_EQ(90000000, video["maxBitRate"].asInt());
  EXPECT_EQ(1000, video["maxDelay"].asInt());
  EXPECT_EQ("144", video["maxDimensions"]["frameRate"].asString());
  EXPECT_EQ(1920, video["maxDimensions"]["width"].asInt());
  EXPECT_EQ(1080, video["maxDimensions"]["height"].asInt());
  EXPECT_DOUBLE_EQ(3.14159, video["maxPixelsPerSecond"].asDouble());
  EXPECT_EQ(3000, video["minBitRate"].asInt());
  EXPECT_EQ("24", video["minDimensions"]["frameRate"].asString());
  EXPECT_EQ(320, video["minDimensions"]["width"].asInt());
  EXPECT_EQ(240, video["minDimensions"]["height"].asInt());
}

TEST_F(ReceiverSessionTest, HandlesNoValidAudioStream) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  StrictMock<FakeClient> client;
  auto environment = MakeEnvironment();
  ReceiverSession session(&client, environment.get(), message_port.get(),
                          ReceiverSession::Preferences{});

  EXPECT_CALL(client, OnNegotiated(&session, _)).Times(1);
  EXPECT_CALL(client, OnConfiguredReceiversDestroyed(&session)).Times(1);

  message_port->ReceiveMessage(kNoAudioOfferMessage);
  const auto& messages = message_port->posted_messages();
  EXPECT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  EXPECT_TRUE(message_body.is_value());
  const Json::Value& answer_body = message_body.value()["answer"];
  EXPECT_TRUE(answer_body.isObject());

  // Should still select video stream.
  EXPECT_EQ(1u, answer_body["sendIndexes"].size());
  EXPECT_EQ(31338, answer_body["sendIndexes"][0].asInt());
  EXPECT_EQ(1u, answer_body["ssrcs"].size());
  EXPECT_EQ(19088746, answer_body["ssrcs"][0].asInt());
}

TEST_F(ReceiverSessionTest, HandlesNoValidVideoStream) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  StrictMock<FakeClient> client;
  auto environment = MakeEnvironment();
  ReceiverSession session(&client, environment.get(), message_port.get(),
                          ReceiverSession::Preferences{});

  EXPECT_CALL(client, OnNegotiated(&session, _)).Times(1);
  EXPECT_CALL(client, OnConfiguredReceiversDestroyed(&session)).Times(1);

  message_port->ReceiveMessage(kNoVideoOfferMessage);
  const auto& messages = message_port->posted_messages();
  EXPECT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  EXPECT_TRUE(message_body.is_value());
  const Json::Value& answer_body = message_body.value()["answer"];
  EXPECT_TRUE(answer_body.isObject());

  // Should still select audio stream.
  EXPECT_EQ(1u, answer_body["sendIndexes"].size());
  EXPECT_EQ(1337, answer_body["sendIndexes"][0].asInt());
  EXPECT_EQ(1u, answer_body["ssrcs"].size());
  EXPECT_EQ(19088748, answer_body["ssrcs"][0].asInt());
}

TEST_F(ReceiverSessionTest, HandlesNoValidStreams) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  StrictMock<FakeClient> client;

  auto environment = MakeEnvironment();
  ReceiverSession session(&client, environment.get(), message_port.get(),
                          ReceiverSession::Preferences{});

  // We shouldn't call OnNegotiated if we failed to negotiate any streams.
  EXPECT_CALL(client, OnNegotiated(&session, _)).Times(0);
  EXPECT_CALL(client, OnConfiguredReceiversDestroyed(&session)).Times(0);

  message_port->ReceiveMessage(kNoAudioOrVideoOfferMessage);
  const auto& messages = message_port->posted_messages();
  EXPECT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  ExpectIsErrorAnswerMessage(message_body);
}

TEST_F(ReceiverSessionTest, HandlesMalformedOffer) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  StrictMock<FakeClient> client;
  auto environment = MakeEnvironment();
  ReceiverSession session(&client, environment.get(), message_port.get(),
                          ReceiverSession::Preferences{});

  // We shouldn't call OnNegotiated if we failed to negotiate any streams.
  // Note that unlike when we simply don't select any streams, when the offer
  // is actually completely invalid we call OnError.
  EXPECT_CALL(client, OnNegotiated(&session, _)).Times(0);
  EXPECT_CALL(client, OnConfiguredReceiversDestroyed(&session)).Times(0);
  EXPECT_CALL(client, OnError(&session, Error(Error::Code::kJsonParseError)))
      .Times(1);

  message_port->ReceiveMessage(kInvalidJsonOfferMessage);
}

TEST_F(ReceiverSessionTest, NotifiesReceiverDestruction) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  StrictMock<FakeClient> client;
  auto environment = MakeEnvironment();
  ReceiverSession session(&client, environment.get(), message_port.get(),
                          ReceiverSession::Preferences{});

  EXPECT_CALL(client, OnNegotiated(&session, _)).Times(2);
  EXPECT_CALL(client, OnConfiguredReceiversDestroyed(&session)).Times(2);

  message_port->ReceiveMessage(kNoAudioOfferMessage);
  message_port->ReceiveMessage(kValidOfferMessage);
}
}  // namespace cast
}  // namespace openscreen
