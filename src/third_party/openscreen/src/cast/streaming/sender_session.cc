// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_session.h"

#include <openssl/rand.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "cast/common/channel/message_util.h"
#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/sender.h"
#include "util/crypto/random_bytes.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

namespace {

AudioStream CreateStream(int index, const AudioCaptureConfig& config) {
  return AudioStream{
      Stream{index,
             Stream::Type::kAudioSource,
             config.channels,
             CodecToString(config.codec),
             GetPayloadType(config.codec),
             GenerateSsrc(true /*high_priority*/),
             config.target_playout_delay,
             GenerateRandomBytes16(),
             GenerateRandomBytes16(),
             false /* receiver_rtcp_event_log */,
             {} /* receiver_rtcp_dscp */,
             config.sample_rate},
      (config.bit_rate >= capture_recommendations::kDefaultAudioMinBitRate)
          ? config.bit_rate
          : capture_recommendations::kDefaultAudioMaxBitRate};
}

Resolution ToResolution(const DisplayResolution& display_resolution) {
  return Resolution{display_resolution.width, display_resolution.height};
}

VideoStream CreateStream(int index, const VideoCaptureConfig& config) {
  std::vector<Resolution> resolutions;
  std::transform(config.resolutions.begin(), config.resolutions.end(),
                 std::back_inserter(resolutions), ToResolution);

  constexpr int kVideoStreamChannelCount = 1;
  return VideoStream{
      Stream{index,
             Stream::Type::kVideoSource,
             kVideoStreamChannelCount,
             CodecToString(config.codec),
             GetPayloadType(config.codec),
             GenerateSsrc(false /*high_priority*/),
             config.target_playout_delay,
             GenerateRandomBytes16(),
             GenerateRandomBytes16(),
             false /* receiver_rtcp_event_log */,
             {} /* receiver_rtcp_dscp */,
             kRtpVideoTimebase},
      SimpleFraction{config.max_frame_rate.numerator,
                     config.max_frame_rate.denominator},
      (config.max_bit_rate >
       capture_recommendations::kDefaultVideoBitRateLimits.minimum)
          ? config.max_bit_rate
          : capture_recommendations::kDefaultVideoBitRateLimits.maximum,
      {},  //  protection
      {},  //  profile
      {},  //  protection
      std::move(resolutions),
      {} /* error_recovery mode, always "castv2" */
  };
}

template <typename S, typename C>
void CreateStreamList(int offset_index,
                      const std::vector<C>& configs,
                      std::vector<S>* out) {
  out->reserve(configs.size());
  for (size_t i = 0; i < configs.size(); ++i) {
    out->emplace_back(CreateStream(i + offset_index, configs[i]));
  }
}

Offer CreateOffer(const std::vector<AudioCaptureConfig>& audio_configs,
                  const std::vector<VideoCaptureConfig>& video_configs) {
  Offer offer{
      {CastMode::Type::kMirroring},
      false /* supports_wifi_status_reporting */,
      {} /* audio_streams */,
      {} /* video_streams */
  };

  // NOTE here: IDs will always follow the pattern:
  // [0.. audio streams... N - 1][N.. video streams.. K]
  CreateStreamList(0, audio_configs, &offer.audio_streams);
  CreateStreamList(audio_configs.size(), video_configs, &offer.video_streams);

  return offer;
}

bool IsValidAudioCaptureConfig(const AudioCaptureConfig& config) {
  return config.channels >= 1 && config.bit_rate >= 0;
}

bool IsValidResolution(const DisplayResolution& resolution) {
  return resolution.width > kMinVideoWidth &&
         resolution.height > kMinVideoHeight;
}

bool IsValidVideoCaptureConfig(const VideoCaptureConfig& config) {
  return config.max_frame_rate.numerator > 0 &&
         config.max_frame_rate.denominator > 0 &&
         ((config.max_bit_rate == 0) ||
          (config.max_bit_rate >=
           capture_recommendations::kDefaultVideoBitRateLimits.minimum)) &&
         !config.resolutions.empty() &&
         std::all_of(config.resolutions.begin(), config.resolutions.end(),
                     IsValidResolution);
}

bool AreAllValid(const std::vector<AudioCaptureConfig>& audio_configs,
                 const std::vector<VideoCaptureConfig>& video_configs) {
  return std::all_of(audio_configs.begin(), audio_configs.end(),
                     IsValidAudioCaptureConfig) &&
         std::all_of(video_configs.begin(), video_configs.end(),
                     IsValidVideoCaptureConfig);
}

}  // namespace

SenderSession::Client::~Client() = default;

SenderSession::SenderSession(IPAddress remote_address,
                             Client* const client,
                             Environment* environment,
                             MessagePort* message_port)
    : remote_address_(remote_address),
      client_(client),
      environment_(environment),
      messager_(message_port,
                MakeUniqueSessionId("sender"),
                [this](Error error) {
                  OSP_DLOG_WARN << "SenderSession message port error: "
                                << error;
                  client_->OnError(this, error);
                }),
      packet_router_(environment_) {
  OSP_DCHECK(client_);
  OSP_DCHECK(environment_);

  messager_.SetHandler(kMessageTypeAnswer,
                       [this](SessionMessager::Message message) {
                         OnAnswer(std::move(message));
                       });
}

SenderSession::~SenderSession() = default;

Error SenderSession::Negotiate(std::vector<AudioCaptureConfig> audio_configs,
                               std::vector<VideoCaptureConfig> video_configs) {
  // Negotiating with no streams doesn't make any sense.
  if (audio_configs.empty() && video_configs.empty()) {
    return Error(Error::Code::kParameterInvalid,
                 "Need at least one audio or video config to negotiate.");
  }
  if (!AreAllValid(audio_configs, video_configs)) {
    return Error(Error::Code::kParameterInvalid, "Invalid configs provided.");
  }

  Offer offer = CreateOffer(audio_configs, video_configs);
  ErrorOr<Json::Value> json_offer = offer.ToJson();
  if (json_offer.is_error()) {
    return std::move(json_offer.error());
  }

  current_negotiation_ = std::unique_ptr<Negotiation>(new Negotiation{
      std::move(offer), std::move(audio_configs), std::move(video_configs)});

  Json::Value message_body;
  message_body[kMessageType] = kMessageTypeOffer;
  message_body[kOfferMessageBody] = std::move(json_offer.value());

  // Currently we don't have a way to discover the ID of the receiver we
  // are connected to, since we have to send the first message.
  // TODO(jophba): migrate to discovered receiver ID when available.
  static constexpr char kPlaceholderReceiverSenderId[] = "receiver-12345";
  messager_.SendMessage(SessionMessager::Message{
      kPlaceholderReceiverSenderId, kCastWebrtcNamespace,
      ++current_sequence_number_, std::move(message_body)});
  return Error::None();
}

void SenderSession::OnAnswer(SessionMessager::Message message) {
  if (message.sequence_number != current_sequence_number_) {
    OSP_DLOG_WARN << "Received a stale answer message, dropping.";
    return;
  } else if (message.sequence_number > current_sequence_number_) {
    OSP_DLOG_WARN
        << "Received an answer with an unexpected sequence number, dropping.";
    return;
  }

  Answer answer;
  if (!Answer::ParseAndValidate(message.body, &answer)) {
    client_->OnError(this, Error(Error::Code::kJsonParseError,
                                 "Received invalid answer message"));
    OSP_DLOG_WARN << "Received invalid answer message";
    return;
  }

  ConfiguredSenders senders = SpawnSenders(answer);

  // If we didn't select any senders, the negotiation was unsuccessful.
  if (senders.audio_sender == nullptr && senders.video_sender == nullptr) {
    return;
  }
  client_->OnNegotiated(this, std::move(senders),
                        capture_recommendations::GetRecommendations(answer));
}

std::unique_ptr<Sender> SenderSession::CreateSender(Ssrc receiver_ssrc,
                                                    const Stream& stream,
                                                    RtpPayloadType type) {
  SessionConfig config{
      stream.ssrc,         receiver_ssrc,  stream.rtp_timebase, stream.channels,
      stream.target_delay, stream.aes_key, stream.aes_iv_mask};

  return std::make_unique<Sender>(environment_, &packet_router_,
                                  std::move(config), type);
}

void SenderSession::SpawnAudioSender(ConfiguredSenders* senders,
                                     Ssrc receiver_ssrc,
                                     int send_index,
                                     int config_index) {
  const AudioCaptureConfig& config =
      current_negotiation_->audio_configs[config_index];
  const RtpPayloadType payload_type = GetPayloadType(config.codec);
  for (const AudioStream& stream : current_negotiation_->offer.audio_streams) {
    if (stream.stream.index == send_index) {
      current_audio_sender_ =
          CreateSender(receiver_ssrc, stream.stream, payload_type);
      senders->audio_sender = current_audio_sender_.get();
      senders->audio_config = config;
      break;
    }
  }
}

void SenderSession::SpawnVideoSender(ConfiguredSenders* senders,
                                     Ssrc receiver_ssrc,
                                     int send_index,
                                     int config_index) {
  const VideoCaptureConfig& config =
      current_negotiation_->video_configs[config_index];
  const RtpPayloadType payload_type = GetPayloadType(config.codec);
  for (const VideoStream& stream : current_negotiation_->offer.video_streams) {
    if (stream.stream.index == send_index) {
      current_video_sender_ =
          CreateSender(receiver_ssrc, stream.stream, payload_type);
      senders->video_sender = current_video_sender_.get();
      senders->video_config = config;
      break;
    }
  }
}

SenderSession::ConfiguredSenders SenderSession::SpawnSenders(
    const Answer& answer) {
  OSP_DCHECK(current_negotiation_);

  // Although we already have a message port set up with the TLS
  // address of the receiver, we don't know where to send the seperate UDP
  // stream until we get the ANSWER message here.
  environment_->set_remote_endpoint(
      IPEndpoint{remote_address_, static_cast<uint16_t>(answer.udp_port)});

  ConfiguredSenders senders;
  for (size_t i = 0; i < answer.send_indexes.size(); ++i) {
    const Ssrc receiver_ssrc = answer.ssrcs[i];
    const size_t send_index = static_cast<size_t>(answer.send_indexes[i]);

    const auto audio_size = current_negotiation_->audio_configs.size();
    const auto video_size = current_negotiation_->video_configs.size();
    if (send_index < audio_size) {
      SpawnAudioSender(&senders, receiver_ssrc, send_index, send_index);
    } else if (send_index < (audio_size + video_size)) {
      SpawnVideoSender(&senders, receiver_ssrc, send_index,
                       send_index - audio_size);
    }
  }
  return senders;
}

}  // namespace cast
}  // namespace openscreen
