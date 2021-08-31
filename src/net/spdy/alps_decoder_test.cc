// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/alps_decoder.h"

#include "net/base/hex_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::spdy::AcceptChOriginValuePair;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

namespace net {
namespace {

TEST(AlpsDecoderTest, EmptyInput) {
  AlpsDecoder decoder;
  EXPECT_THAT(decoder.GetAcceptCh(), IsEmpty());
  EXPECT_THAT(decoder.GetSettings(), IsEmpty());
  EXPECT_EQ(0, decoder.settings_frame_count());

  AlpsDecoder::Error error = decoder.Decode({});
  EXPECT_EQ(AlpsDecoder::Error::kNoError, error);

  EXPECT_THAT(decoder.GetAcceptCh(), IsEmpty());
  EXPECT_THAT(decoder.GetSettings(), IsEmpty());
  EXPECT_EQ(0, decoder.settings_frame_count());
}

TEST(AlpsDecoderTest, EmptyAcceptChFrame) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("000000"       // length
                               "89"           // type ACCEPT_CH
                               "00"           // flags
                               "00000000"));  // stream ID

  EXPECT_EQ(AlpsDecoder::Error::kNoError, error);
  EXPECT_THAT(decoder.GetAcceptCh(), IsEmpty());
  EXPECT_THAT(decoder.GetSettings(), IsEmpty());
  EXPECT_EQ(0, decoder.settings_frame_count());
}

TEST(AlpsDecoderTest, EmptySettingsFrame) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("000000"       // length
                               "04"           // type SETTINGS
                               "00"           // flags
                               "00000000"));  // stream ID

  EXPECT_EQ(AlpsDecoder::Error::kNoError, error);
  EXPECT_THAT(decoder.GetAcceptCh(), IsEmpty());
  EXPECT_THAT(decoder.GetSettings(), IsEmpty());
  EXPECT_EQ(1, decoder.settings_frame_count());
}

TEST(AlpsDecoderTest, ParseSettingsAndAcceptChFrames) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error = decoder.Decode(HexDecode(
      // ACCEPT_CH frame
      "00003d"                    // length
      "89"                        // type ACCEPT_CH
      "00"                        // flags
      "00000000"                  // stream ID
      "0017"                      // origin length
      "68747470733a2f2f7777772e"  //
      "6578616d706c652e636f6d"    // origin "https://www.example.com"
      "0003"                      // value length
      "666f6f"                    // value "foo"
      "0018"                      // origin length
      "68747470733a2f2f6d61696c"  //
      "2e6578616d706c652e636f6d"  // origin "https://mail.example.com"
      "0003"                      // value length
      "626172"                    // value "bar"
      // SETTINGS frame
      "00000c"       // length
      "04"           // type
      "00"           // flags
      "00000000"     // stream ID
      "0dab"         // identifier
      "01020304"     // value
      "1234"         // identifier
      "fedcba98"));  // value

  EXPECT_EQ(AlpsDecoder::Error::kNoError, error);
  EXPECT_THAT(
      decoder.GetAcceptCh(),
      ElementsAre(AcceptChOriginValuePair{"https://www.example.com", "foo"},
                  AcceptChOriginValuePair{"https://mail.example.com", "bar"}));
  EXPECT_THAT(decoder.GetSettings(),
              ElementsAre(Pair(0x0dab, 0x01020304), Pair(0x1234, 0xfedcba98)));
  EXPECT_EQ(1, decoder.settings_frame_count());
}

TEST(AlpsDecoderTest, IncompleteFrame) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("00000c"    // length
                               "04"        // type
                               "00"        // flags
                               "00000000"  // stream ID
                               "0dab"      // identifier
                               "01"));     // first byte of value

  EXPECT_EQ(AlpsDecoder::Error::kNotOnFrameBoundary, error);
}

TEST(AlpsDecoderTest, TwoSettingsFrames) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("000006"       // length
                               "04"           // type SETTINGS
                               "00"           // flags
                               "00000000"     // stream ID
                               "0dab"         // identifier
                               "01020304"     // value
                               "000006"       // length
                               "04"           // type SETTINGS
                               "00"           // flags
                               "00000000"     // stream ID
                               "1234"         // identifier
                               "fedcba98"));  // value

  EXPECT_EQ(AlpsDecoder::Error::kNoError, error);
  EXPECT_EQ(2, decoder.settings_frame_count());
  EXPECT_THAT(decoder.GetSettings(),
              ElementsAre(Pair(0x0dab, 0x01020304), Pair(0x1234, 0xfedcba98)));
}

TEST(AlpsDecoderTest, AcceptChOnInvalidStream) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error = decoder.Decode(
      HexDecode("00001e"                    // length
                "89"                        // type ACCEPT_CH
                "00"                        // flags
                "00000001"                  // invalid stream ID: should be zero
                "0017"                      // origin length
                "68747470733a2f2f7777772e"  //
                "6578616d706c652e636f6d"    // origin "https://www.example.com"
                "0003"                      // value length
                "666f6f"));                 // value "foo"

  EXPECT_EQ(AlpsDecoder::Error::kAcceptChInvalidStream, error);
}

// According to
// https://davidben.github.io/http-client-hint-reliability/ \
// draft-davidben-http-client-hint-reliability.html#name-http-2-accept_ch-frame
// "If a user agent receives an ACCEPT_CH frame whose stream [...] flags
// field is non-zero, it MUST respond with a connection error [...]."
TEST(AlpsDecoderTest, AcceptChWithInvalidFlags) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error = decoder.Decode(
      HexDecode("00001e"                    // length
                "89"                        // type ACCEPT_CH
                "02"                        // invalid flags: should be zero
                "00000000"                  // stream ID
                "0017"                      // origin length
                "68747470733a2f2f7777772e"  //
                "6578616d706c652e636f6d"    // origin "https://www.example.com"
                "0003"                      // value length
                "666f6f"));                 // value "foo"

  EXPECT_EQ(AlpsDecoder::Error::kAcceptChWithFlags, error);
}

TEST(AlpsDecoderTest, SettingsOnInvalidStream) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("000006"    // length
                               "04"        // type SETTINGS
                               "00"        // flags
                               "00000001"  // invalid stream ID: should be zero
                               "1234"      // identifier
                               "fedcba98"));  // value

  EXPECT_EQ(AlpsDecoder::Error::kFramingError, error);
}

TEST(AlpsDecoderTest, SettingsAck) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("000000"       // length
                               "04"           // type SETTINGS
                               "01"           // ACK flag
                               "00000000"));  // stream ID

  EXPECT_EQ(AlpsDecoder::Error::kSettingsWithAck, error);
}

// According to https://httpwg.org/specs/rfc7540.html#FrameHeader:
// "Flags that have no defined semantics for a particular frame type MUST be
// ignored [...]"
TEST(AlpsDecoderTest, SettingsWithInvalidFlags) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("000006"       // length
                               "04"           // type SETTINGS
                               "02"           // invalid flag
                               "00000000"     // stream ID
                               "1234"         // identifier
                               "fedcba98"));  // value

  EXPECT_EQ(AlpsDecoder::Error::kNoError, error);
}

TEST(AlpsDecoderTest, ForbiddenFrame) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("000003"     // length
                               "00"         // frame type DATA
                               "01"         // flags END_STREAM
                               "00000001"   // stream ID
                               "666f6f"));  // payload "foo"

  EXPECT_EQ(AlpsDecoder::Error::kForbiddenFrame, error);
}

TEST(AlpsDecoderTest, UnknownFrame) {
  AlpsDecoder decoder;
  AlpsDecoder::Error error =
      decoder.Decode(HexDecode("000003"     // length
                               "2a"         // unknown frame type
                               "ff"         // flags
                               "00000008"   // stream ID
                               "666f6f"));  // payload "foo"

  EXPECT_EQ(AlpsDecoder::Error::kNoError, error);
  EXPECT_THAT(decoder.GetAcceptCh(), IsEmpty());
  EXPECT_THAT(decoder.GetSettings(), IsEmpty());
  EXPECT_EQ(0, decoder.settings_frame_count());
}

TEST(AlpsDecoderTest, MalformedAcceptChFrame) {
  // Correct, complete payload.
  std::string payload = HexDecode(
      "0017"  // origin length
      "68747470733a2f2f7777772e"
      "6578616d706c652e636f6d"  // origin "https://www.example.com"
      "0003"                    // value length
      "666f6f");                // value "foo"

  for (uint8_t payload_length = 1; payload_length < payload.length();
       payload_length++) {
    // First two bytes of length.
    std::string frame = HexDecode("0000");
    // Last byte of length.
    frame.push_back(static_cast<char>(payload_length));

    frame.append(
        HexDecode("89"           // type ACCEPT_CH
                  "00"           // flags
                  "00000000"));  // stream ID
    // Incomplete, malformed payload.
    frame.append(payload.data(), payload_length);

    AlpsDecoder decoder;
    AlpsDecoder::Error error = decoder.Decode(frame);

    EXPECT_EQ(AlpsDecoder::Error::kAcceptChMalformed, error);
  }
}

}  // namespace
}  // namespace net
