// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer_messages.h"

#include <inttypes.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/constants.h"
#include "platform/base/error.h"
#include "util/big_endian.h"
#include "util/enum_name_table.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {

namespace {

constexpr char kSupportedStreams[] = "supportedStreams";
constexpr char kAudioSourceType[] = "audio_source";
constexpr char kVideoSourceType[] = "video_source";
constexpr char kStreamType[] = "type";

ErrorOr<RtpPayloadType> ParseRtpPayloadType(const Json::Value& parent,
                                            const std::string& field) {
  auto t = json::ParseInt(parent, field);
  if (!t) {
    return t.error();
  }

  uint8_t t_small = t.value();
  if (t_small != t.value() || !IsRtpPayloadType(t_small)) {
    return Error(Error::Code::kParameterInvalid,
                 "Received invalid RTP Payload Type.");
  }

  return static_cast<RtpPayloadType>(t_small);
}

ErrorOr<int> ParseRtpTimebase(const Json::Value& parent,
                              const std::string& field) {
  auto error_or_raw = json::ParseString(parent, field);
  if (!error_or_raw) {
    return error_or_raw.error();
  }

  // The spec demands a leading 1, so this isn't really a fraction.
  const auto fraction = SimpleFraction::FromString(error_or_raw.value());
  if (fraction.is_error() || !fraction.value().is_positive() ||
      fraction.value().numerator != 1) {
    return json::CreateParseError("RTP timebase");
  }
  return fraction.value().denominator;
}

// For a hex byte, the conversion is 4 bits to 1 character, e.g.
// 0b11110001 becomes F1, so 1 byte is two characters.
constexpr int kHexDigitsPerByte = 2;
constexpr int kAesBytesSize = 16;
constexpr int kAesStringLength = kAesBytesSize * kHexDigitsPerByte;
ErrorOr<std::array<uint8_t, kAesBytesSize>> ParseAesHexBytes(
    const Json::Value& parent,
    const std::string& field) {
  auto hex_string = json::ParseString(parent, field);
  if (!hex_string) {
    return hex_string.error();
  }

  constexpr int kHexDigitsPerScanField = 16;
  constexpr int kNumScanFields = kAesStringLength / kHexDigitsPerScanField;
  uint64_t quads[kNumScanFields];
  int chars_scanned;
  if (hex_string.value().size() == kAesStringLength &&
      sscanf(hex_string.value().c_str(), "%16" SCNx64 "%16" SCNx64 "%n",
             &quads[0], &quads[1], &chars_scanned) == kNumScanFields &&
      chars_scanned == kAesStringLength &&
      std::none_of(hex_string.value().begin(), hex_string.value().end(),
                   [](char c) { return std::isspace(c); })) {
    std::array<uint8_t, kAesBytesSize> bytes;
    WriteBigEndian(quads[0], bytes.data());
    WriteBigEndian(quads[1], bytes.data() + 8);
    return bytes;
  }
  return json::CreateParseError("AES hex string bytes");
}

ErrorOr<Stream> ParseStream(const Json::Value& value, Stream::Type type) {
  auto index = json::ParseInt(value, "index");
  if (!index) {
    return index.error();
  }
  // If channel is omitted, the default value is used later.
  auto channels = json::ParseInt(value, "channels");
  if (channels.is_value() && channels.value() <= 0) {
    return json::CreateParameterError("channel");
  }
  auto rtp_profile = json::ParseString(value, "rtpProfile");
  if (!rtp_profile) {
    return rtp_profile.error();
  }
  auto rtp_payload_type = ParseRtpPayloadType(value, "rtpPayloadType");
  if (!rtp_payload_type) {
    return rtp_payload_type.error();
  }
  auto ssrc = json::ParseUint(value, "ssrc");
  if (!ssrc) {
    return ssrc.error();
  }
  auto aes_key = ParseAesHexBytes(value, "aesKey");
  auto aes_iv_mask = ParseAesHexBytes(value, "aesIvMask");
  if (!aes_key || !aes_iv_mask) {
    return Error(Error::Code::kUnencryptedOffer,
                 "Offer stream must have both a valid aesKey and aesIvMask");
  }
  auto rtp_timebase = ParseRtpTimebase(value, "timeBase");
  if (!rtp_timebase) {
    return rtp_timebase.error();
  }
  if (rtp_timebase.value() <
          std::min(capture_recommendations::kDefaultAudioMinSampleRate,
                   kRtpVideoTimebase) ||
      rtp_timebase.value() > kRtpVideoTimebase) {
    return json::CreateParameterError("rtp_timebase (sample rate)");
  }

  auto target_delay = json::ParseInt(value, "targetDelay");
  std::chrono::milliseconds target_delay_ms = kDefaultTargetPlayoutDelay;
  if (target_delay) {
    auto d = std::chrono::milliseconds(target_delay.value());
    if (kMinTargetPlayoutDelay <= d && d <= kMaxTargetPlayoutDelay) {
      target_delay_ms = d;
    }
  }

  auto receiver_rtcp_event_log = json::ParseBool(value, "receiverRtcpEventLog");
  auto receiver_rtcp_dscp = json::ParseString(value, "receiverRtcpDscp");
  return Stream{index.value(),
                type,
                channels.value(type == Stream::Type::kAudioSource
                                   ? kDefaultNumAudioChannels
                                   : kDefaultNumVideoChannels),
                rtp_payload_type.value(),
                ssrc.value(),
                target_delay_ms,
                aes_key.value(),
                aes_iv_mask.value(),
                receiver_rtcp_event_log.value({}),
                receiver_rtcp_dscp.value({}),
                rtp_timebase.value()};
}

ErrorOr<AudioStream> ParseAudioStream(const Json::Value& value) {
  auto stream = ParseStream(value, Stream::Type::kAudioSource);
  if (!stream) {
    return stream.error();
  }
  auto bit_rate = json::ParseInt(value, "bitRate");
  if (!bit_rate) {
    return bit_rate.error();
  }

  auto codec_name = json::ParseString(value, "codecName");
  if (!codec_name) {
    return codec_name.error();
  }
  ErrorOr<AudioCodec> codec = StringToAudioCodec(codec_name.value());
  if (!codec) {
    return Error(Error::Code::kUnknownCodec,
                 "Codec is not known, can't use stream");
  }

  // A bit rate of 0 is valid for some codec types, so we don't enforce here.
  if (bit_rate.value() < 0) {
    return json::CreateParameterError("bit rate");
  }
  return AudioStream{stream.value(), codec.value(), bit_rate.value()};
}

ErrorOr<Resolution> ParseResolution(const Json::Value& value) {
  auto width = json::ParseInt(value, "width");
  if (!width) {
    return width.error();
  }
  auto height = json::ParseInt(value, "height");
  if (!height) {
    return height.error();
  }
  if (width.value() <= 0 || height.value() <= 0) {
    return json::CreateParameterError("resolution");
  }
  return Resolution{width.value(), height.value()};
}

ErrorOr<std::vector<Resolution>> ParseResolutions(const Json::Value& parent,
                                                  const std::string& field) {
  std::vector<Resolution> resolutions;
  // Some legacy senders don't provide resolutions, so just return empty.
  const Json::Value& value = parent[field];
  if (!value.isArray() || value.empty()) {
    return resolutions;
  }

  for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
    auto r = ParseResolution(value[i]);
    if (!r) {
      return r.error();
    }
    resolutions.push_back(r.value());
  }

  return resolutions;
}

ErrorOr<VideoStream> ParseVideoStream(const Json::Value& value) {
  auto stream = ParseStream(value, Stream::Type::kVideoSource);
  if (!stream) {
    return stream.error();
  }
  auto codec_name = json::ParseString(value, "codecName");
  if (!codec_name) {
    return codec_name.error();
  }
  ErrorOr<VideoCodec> codec = StringToVideoCodec(codec_name.value());
  if (!codec) {
    return Error(Error::Code::kUnknownCodec,
                 "Codec is not known, can't use stream");
  }
  auto resolutions = ParseResolutions(value, "resolutions");
  if (!resolutions) {
    return resolutions.error();
  }

  auto raw_max_frame_rate = json::ParseString(value, "maxFrameRate");
  SimpleFraction max_frame_rate{kDefaultMaxFrameRate, 1};
  if (raw_max_frame_rate.is_value()) {
    auto parsed = SimpleFraction::FromString(raw_max_frame_rate.value());
    if (parsed.is_value() && parsed.value().is_positive()) {
      max_frame_rate = parsed.value();
    }
  }

  auto profile = json::ParseString(value, "profile");
  auto protection = json::ParseString(value, "protection");
  auto max_bit_rate = json::ParseInt(value, "maxBitRate");
  auto level = json::ParseString(value, "level");
  auto error_recovery_mode = json::ParseString(value, "errorRecoveryMode");
  return VideoStream{stream.value(),
                     codec.value(),
                     max_frame_rate,
                     max_bit_rate.value(4 << 20),
                     protection.value({}),
                     profile.value({}),
                     level.value({}),
                     resolutions.value(),
                     error_recovery_mode.value({})};
}

absl::string_view ToString(Stream::Type type) {
  switch (type) {
    case Stream::Type::kAudioSource:
      return kAudioSourceType;
    case Stream::Type::kVideoSource:
      return kVideoSourceType;
    default: {
      OSP_NOTREACHED();
    }
  }
}

EnumNameTable<CastMode, 2> kCastModeNames{
    {{"mirroring", CastMode::kMirroring}, {"remoting", CastMode::kRemoting}}};

}  // namespace

ErrorOr<Json::Value> Stream::ToJson() const {
  if (channels < 1 || index < 0 || target_delay.count() <= 0 ||
      target_delay.count() > std::numeric_limits<int>::max() ||
      rtp_timebase < 1) {
    return json::CreateParameterError("Stream");
  }

  Json::Value root;
  root["index"] = index;
  root["type"] = std::string(ToString(type));
  root["channels"] = channels;
  root["rtpPayloadType"] = static_cast<int>(rtp_payload_type);
  // rtpProfile is technically required by the spec, although it is always set
  // to cast. We set it here to be compliant with all spec implementers.
  root["rtpProfile"] = "cast";
  static_assert(sizeof(ssrc) <= sizeof(Json::UInt),
                "this code assumes Ssrc fits in a Json::UInt");
  root["ssrc"] = static_cast<Json::UInt>(ssrc);
  root["targetDelay"] = static_cast<int>(target_delay.count());
  root["aesKey"] = HexEncode(aes_key);
  root["aesIvMask"] = HexEncode(aes_iv_mask);
  root["receiverRtcpEventLog"] = receiver_rtcp_event_log;
  root["receiverRtcpDscp"] = receiver_rtcp_dscp;
  root["timeBase"] = "1/" + std::to_string(rtp_timebase);
  return root;
}

ErrorOr<Json::Value> AudioStream::ToJson() const {
  // A bit rate of 0 is valid for some codec types, so we don't enforce here.
  if (bit_rate < 0) {
    return json::CreateParameterError("AudioStream");
  }

  auto error_or_stream = stream.ToJson();
  if (error_or_stream.is_error()) {
    return error_or_stream;
  }

  error_or_stream.value()["codecName"] = CodecToString(codec);
  error_or_stream.value()["bitRate"] = bit_rate;
  return error_or_stream;
}

ErrorOr<Json::Value> Resolution::ToJson() const {
  if (width <= 0 || height <= 0) {
    return json::CreateParameterError("Resolution");
  }

  Json::Value root;
  root["width"] = width;
  root["height"] = height;
  return root;
}

ErrorOr<Json::Value> VideoStream::ToJson() const {
  if (max_bit_rate <= 0 || !max_frame_rate.is_positive()) {
    return json::CreateParameterError("VideoStream");
  }

  auto error_or_stream = stream.ToJson();
  if (error_or_stream.is_error()) {
    return error_or_stream;
  }

  auto& stream = error_or_stream.value();
  stream["codecName"] = CodecToString(codec);
  stream["maxFrameRate"] = max_frame_rate.ToString();
  stream["maxBitRate"] = max_bit_rate;
  stream["protection"] = protection;
  stream["profile"] = profile;
  stream["level"] = level;
  stream["errorRecoveryMode"] = error_recovery_mode;

  Json::Value rs;
  for (auto resolution : resolutions) {
    auto eoj = resolution.ToJson();
    if (eoj.is_error()) {
      return eoj;
    }
    rs.append(eoj.value());
  }
  stream["resolutions"] = std::move(rs);
  return error_or_stream;
}

// static
ErrorOr<Offer> Offer::Parse(const Json::Value& root) {
  if (!root.isObject()) {
    return json::CreateParseError("null offer");
  }
  ErrorOr<CastMode> cast_mode =
      GetEnum(kCastModeNames, root["castMode"].asString());
  const ErrorOr<bool> get_status = json::ParseBool(root, "receiverGetStatus");

  Json::Value supported_streams = root[kSupportedStreams];
  if (!supported_streams.isArray()) {
    return json::CreateParseError("supported streams in offer");
  }

  std::vector<AudioStream> audio_streams;
  std::vector<VideoStream> video_streams;
  for (Json::ArrayIndex i = 0; i < supported_streams.size(); ++i) {
    const Json::Value& fields = supported_streams[i];
    auto type = json::ParseString(fields, kStreamType);
    if (!type) {
      return type.error();
    }

    if (type.value() == kAudioSourceType) {
      auto stream = ParseAudioStream(fields);
      if (!stream) {
        if (stream.error().code() == Error::Code::kUnknownCodec) {
          OSP_DVLOG << "Dropping audio stream due to unknown codec: "
                    << stream.error();
          continue;
        } else {
          return stream.error();
        }
      }
      audio_streams.push_back(std::move(stream.value()));
    } else if (type.value() == kVideoSourceType) {
      auto stream = ParseVideoStream(fields);
      if (!stream) {
        if (stream.error().code() == Error::Code::kUnknownCodec) {
          OSP_DVLOG << "Dropping video stream due to unknown codec: "
                    << stream.error();
          continue;
        } else {
          return stream.error();
        }
      }
      video_streams.push_back(std::move(stream.value()));
    }
  }

  return Offer{cast_mode.value(CastMode::kMirroring), get_status.value({}),
               std::move(audio_streams), std::move(video_streams)};
}

ErrorOr<Json::Value> Offer::ToJson() const {
  Json::Value root;

  root["castMode"] = GetEnumName(kCastModeNames, cast_mode).value();
  root["receiverGetStatus"] = supports_wifi_status_reporting;

  Json::Value streams;
  for (auto& as : audio_streams) {
    auto eoj = as.ToJson();
    if (eoj.is_error()) {
      return eoj;
    }
    streams.append(eoj.value());
  }

  for (auto& vs : video_streams) {
    auto eoj = vs.ToJson();
    if (eoj.is_error()) {
      return eoj;
    }
    streams.append(eoj.value());
  }

  root[kSupportedStreams] = std::move(streams);
  return root;
}

}  // namespace cast
}  // namespace openscreen
