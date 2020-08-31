// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_OFFER_MESSAGES_H_
#define CAST_STREAMING_OFFER_MESSAGES_H_

#include <chrono>  // NOLINT
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "cast/streaming/rtp_defines.h"
#include "cast/streaming/session_config.h"
#include "json/value.h"
#include "platform/base/error.h"
#include "util/simple_fraction.h"

// This file contains the implementation of the Cast V2 Mirroring Control
// Protocol offer object definition.
namespace openscreen {
namespace cast {

// If the target delay provided by the sender is not bounded by
// [kMinTargetDelay, kMaxTargetDelay], it will be set to
// kDefaultTargetPlayoutDelay.
constexpr auto kMinTargetPlayoutDelay = std::chrono::milliseconds(0);
constexpr auto kMaxTargetPlayoutDelay = std::chrono::milliseconds(2000);

// If the sender provides an invalid maximum frame rate, it will
// be set to kDefaultMaxFrameRate.
constexpr int kDefaultMaxFrameRate = 30;

constexpr int kDefaultNumVideoChannels = 1;
constexpr int kDefaultNumAudioChannels = 2;

// A stream, as detailed by the CastV2 protocol spec, is a segment of an
// offer message specifically representing a configuration object for
// a codec and its related fields, such as maximum bit rate, time base,
// and other fields.
// Composed classes include AudioStream and VideoStream, which contain
// fields specific to audio and video respectively.
struct Stream {
  enum class Type : uint8_t { kAudioSource, kVideoSource };

  ErrorOr<Json::Value> ToJson() const;

  int index = 0;
  Type type = {};

  // Default channel count is 1, e.g. for video.
  int channels = 0;
  std::string codec_name = {};
  RtpPayloadType rtp_payload_type = {};
  Ssrc ssrc = {};
  std::chrono::milliseconds target_delay = {};

  // AES Key and IV mask format is very strict: a 32 digit hex string that
  // must be converted to a 16 digit byte array.
  std::array<uint8_t, 16> aes_key = {};
  std::array<uint8_t, 16> aes_iv_mask = {};
  bool receiver_rtcp_event_log = {};
  std::string receiver_rtcp_dscp = {};
  int rtp_timebase = 0;
};

struct AudioStream {
  ErrorOr<Json::Value> ToJson() const;

  Stream stream = {};
  int bit_rate = 0;
};

struct Resolution {
  ErrorOr<Json::Value> ToJson() const;

  int width = 0;
  int height = 0;
};

struct VideoStream {
  ErrorOr<Json::Value> ToJson() const;

  Stream stream = {};
  SimpleFraction max_frame_rate;
  int max_bit_rate = 0;
  std::string protection = {};
  std::string profile = {};
  std::string level = {};
  std::vector<Resolution> resolutions = {};
  std::string error_recovery_mode = {};
};

struct CastMode {
 public:
  enum class Type : uint8_t { kMirroring, kRemoting };

  static CastMode Parse(absl::string_view value);
  std::string ToString() const;

  // Default cast mode is mirroring.
  Type type = Type::kMirroring;
};

struct Offer {
  static ErrorOr<Offer> Parse(const Json::Value& root);
  ErrorOr<Json::Value> ToJson() const;

  CastMode cast_mode = {};
  // This field is poorly named in the spec (receiverGetStatus), so we use
  // a more descriptive name here.
  bool supports_wifi_status_reporting = {};
  std::vector<AudioStream> audio_streams = {};
  std::vector<VideoStream> video_streams = {};
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_OFFER_MESSAGES_H_
