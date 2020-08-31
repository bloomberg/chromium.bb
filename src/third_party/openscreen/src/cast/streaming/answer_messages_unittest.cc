// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/answer_messages.h"

#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

namespace {

const Answer kValidAnswer{
    CastMode{CastMode::Type::kMirroring},
    1234,                         // udp_port
    std::vector<int>{1, 2, 3},    // send_indexes
    std::vector<Ssrc>{123, 456},  // ssrcs
    Constraints{
        AudioConstraints{
            96000,                           // max_sample_rate
            7,                               // max_channels
            32000,                           // min_bit_rate
            96000,                           // max_bit_rate
            std::chrono::milliseconds(2000)  // max_delay
        },                                   // audio
        VideoConstraints{
            40000.0,  // max_pixels_per_second
            Dimensions{
                320,                        // width
                480,                        // height
                SimpleFraction{15000, 101}  // frame_rate
            },                              // min_dimensions
            Dimensions{
                1920,                   // width
                1080,                   // height
                SimpleFraction{288, 2}  // frame_rate
            },
            300000,                          // min_bit_rate
            144000000,                       // max_bit_rate
            std::chrono::milliseconds(3000)  // max_delay
        }                                    // video
    },                                       // constraints
    DisplayDescription{
        Dimensions{
            640,                   // width
            480,                   // height
            SimpleFraction{30, 1}  // frame_rate
        },
        AspectRatio{16, 9},             // aspect_ratio
        AspectRatioConstraint::kFixed,  // scaling
    },
    std::vector<int>{7, 8, 9},              // receiver_rtcp_event_log
    std::vector<int>{11, 12, 13},           // receiver_rtcp_dscp
    true,                                   // receiver_get_status
    std::vector<std::string>{"foo", "bar"}  // rtp_extensions
};

}  // anonymous namespace

TEST(AnswerMessagesTest, ProperlyPopulatedAnswerSerializesProperly) {
  auto value_or_error = kValidAnswer.ToJson();
  EXPECT_TRUE(value_or_error.is_value());

  Json::Value root = std::move(value_or_error.value());
  EXPECT_EQ(root["castMode"], "mirroring");
  EXPECT_EQ(root["udpPort"], 1234);

  Json::Value sendIndexes = std::move(root["sendIndexes"]);
  EXPECT_EQ(sendIndexes.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(sendIndexes[0], 1);
  EXPECT_EQ(sendIndexes[1], 2);
  EXPECT_EQ(sendIndexes[2], 3);

  Json::Value ssrcs = std::move(root["ssrcs"]);
  EXPECT_EQ(ssrcs.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(ssrcs[0], 123u);
  EXPECT_EQ(ssrcs[1], 456u);

  Json::Value constraints = std::move(root["constraints"]);
  Json::Value audio = std::move(constraints["audio"]);
  EXPECT_EQ(audio.type(), Json::ValueType::objectValue);
  EXPECT_EQ(audio["maxSampleRate"], 96000);
  EXPECT_EQ(audio["maxChannels"], 7);
  EXPECT_EQ(audio["minBitRate"], 32000);
  EXPECT_EQ(audio["maxBitRate"], 96000);
  EXPECT_EQ(audio["maxDelay"], 2000);

  Json::Value video = std::move(constraints["video"]);
  EXPECT_EQ(video.type(), Json::ValueType::objectValue);
  EXPECT_EQ(video["maxPixelsPerSecond"], 40000.0);
  EXPECT_EQ(video["minBitRate"], 300000);
  EXPECT_EQ(video["maxBitRate"], 144000000);
  EXPECT_EQ(video["maxDelay"], 3000);

  Json::Value min_dimensions = std::move(video["minDimensions"]);
  EXPECT_EQ(min_dimensions.type(), Json::ValueType::objectValue);
  EXPECT_EQ(min_dimensions["width"], 320);
  EXPECT_EQ(min_dimensions["height"], 480);
  EXPECT_EQ(min_dimensions["frameRate"], "15000/101");

  Json::Value max_dimensions = std::move(video["maxDimensions"]);
  EXPECT_EQ(max_dimensions.type(), Json::ValueType::objectValue);
  EXPECT_EQ(max_dimensions["width"], 1920);
  EXPECT_EQ(max_dimensions["height"], 1080);
  EXPECT_EQ(max_dimensions["frameRate"], "288/2");

  Json::Value display = std::move(root["display"]);
  EXPECT_EQ(display.type(), Json::ValueType::objectValue);
  EXPECT_EQ(display["aspectRatio"], "16:9");
  EXPECT_EQ(display["scaling"], "sender");

  Json::Value dimensions = std::move(display["dimensions"]);
  EXPECT_EQ(dimensions.type(), Json::ValueType::objectValue);
  EXPECT_EQ(dimensions["width"], 640);
  EXPECT_EQ(dimensions["height"], 480);
  EXPECT_EQ(dimensions["frameRate"], "30");

  Json::Value receiver_rtcp_event_log = std::move(root["receiverRtcpEventLog"]);
  EXPECT_EQ(receiver_rtcp_event_log.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(receiver_rtcp_event_log[0], 7);
  EXPECT_EQ(receiver_rtcp_event_log[1], 8);
  EXPECT_EQ(receiver_rtcp_event_log[2], 9);

  Json::Value receiver_rtcp_dscp = std::move(root["receiverRtcpDscp"]);
  EXPECT_EQ(receiver_rtcp_dscp.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(receiver_rtcp_dscp[0], 11);
  EXPECT_EQ(receiver_rtcp_dscp[1], 12);
  EXPECT_EQ(receiver_rtcp_dscp[2], 13);

  EXPECT_EQ(root["receiverGetStatus"], true);

  Json::Value rtp_extensions = std::move(root["rtpExtensions"]);
  EXPECT_EQ(rtp_extensions.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(rtp_extensions[0], "foo");
  EXPECT_EQ(rtp_extensions[1], "bar");
}

TEST(AnswerMessagesTest, InvalidDimensionsCauseError) {
  Answer invalid_dimensions = kValidAnswer;
  invalid_dimensions.display.value().dimensions.width = -1;
  auto value_or_error = invalid_dimensions.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, InvalidAudioConstraintsCauseError) {
  Answer invalid_audio = kValidAnswer;
  invalid_audio.constraints.value().audio.max_bit_rate =
      invalid_audio.constraints.value().audio.min_bit_rate - 1;
  auto value_or_error = invalid_audio.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, InvalidVideoConstraintsCauseError) {
  Answer invalid_video = kValidAnswer;
  invalid_video.constraints.value().video.max_pixels_per_second = -1.0;
  auto value_or_error = invalid_video.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, InvalidDisplayDescriptionsCauseError) {
  Answer invalid_display = kValidAnswer;
  invalid_display.display.value().aspect_ratio = {0, 0};
  auto value_or_error = invalid_display.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, InvalidUdpPortsCauseError) {
  Answer invalid_port = kValidAnswer;
  invalid_port.udp_port = 65536;
  auto value_or_error = invalid_port.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

}  // namespace cast
}  // namespace openscreen
