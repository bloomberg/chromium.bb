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
#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/sender.h"
#include "cast/streaming/sender_message.h"
#include "util/crypto/random_bytes.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {

namespace {

AudioStream CreateStream(int index,
                         const AudioCaptureConfig& config,
                         bool use_android_rtp_hack) {
  return AudioStream{Stream{index,
                            Stream::Type::kAudioSource,
                            config.channels,
                            GetPayloadType(config.codec, use_android_rtp_hack),
                            GenerateSsrc(true /*high_priority*/),
                            config.target_playout_delay,
                            GenerateRandomBytes16(),
                            GenerateRandomBytes16(),
                            false /* receiver_rtcp_event_log */,
                            {} /* receiver_rtcp_dscp */,
                            config.sample_rate,
                            config.codec_parameter},
                     config.codec,
                     std::max(config.bit_rate, kDefaultAudioMinBitRate)};
}

VideoStream CreateStream(int index,
                         const VideoCaptureConfig& config,
                         bool use_android_rtp_hack) {
  constexpr int kVideoStreamChannelCount = 1;
  return VideoStream{
      Stream{index,
             Stream::Type::kVideoSource,
             kVideoStreamChannelCount,
             GetPayloadType(config.codec, use_android_rtp_hack),
             GenerateSsrc(false /*high_priority*/),
             config.target_playout_delay,
             GenerateRandomBytes16(),
             GenerateRandomBytes16(),
             false /* receiver_rtcp_event_log */,
             {} /* receiver_rtcp_dscp */,
             kRtpVideoTimebase,
             config.codec_parameter},
      config.codec,
      config.max_frame_rate,
      (config.max_bit_rate >= kDefaultVideoMinBitRate)
          ? config.max_bit_rate
          : kDefaultVideoMaxBitRate,
      {},  //  protection
      {},  //  profile
      {},  //  protection
      config.resolutions,
      {} /* error_recovery mode, always "castv2" */
  };
}

template <typename S, typename C>
void CreateStreamList(int offset_index,
                      const std::vector<C>& configs,
                      bool use_android_rtp_hack,
                      std::vector<S>* out) {
  out->reserve(configs.size());
  for (size_t i = 0; i < configs.size(); ++i) {
    out->emplace_back(
        CreateStream(i + offset_index, configs[i], use_android_rtp_hack));
  }
}

Offer CreateMirroringOffer(const std::vector<AudioCaptureConfig>& audio_configs,
                           const std::vector<VideoCaptureConfig>& video_configs,
                           bool use_android_rtp_hack) {
  Offer offer;
  offer.cast_mode = CastMode::kMirroring;

  // NOTE here: IDs will always follow the pattern:
  // [0.. audio streams... N - 1][N.. video streams.. K]
  CreateStreamList(0, audio_configs, use_android_rtp_hack,
                   &offer.audio_streams);
  CreateStreamList(audio_configs.size(), video_configs, use_android_rtp_hack,
                   &offer.video_streams);

  return offer;
}

Offer CreateRemotingOffer(const AudioCaptureConfig& audio_config,
                          const VideoCaptureConfig& video_config,
                          bool use_android_rtp_hack) {
  Offer offer;
  offer.cast_mode = CastMode::kRemoting;

  AudioStream audio_stream =
      CreateStream(0, audio_config, use_android_rtp_hack);
  audio_stream.codec = AudioCodec::kNotSpecified;
  audio_stream.stream.rtp_payload_type =
      GetPayloadType(AudioCodec::kNotSpecified, use_android_rtp_hack);
  offer.audio_streams.push_back(std::move(audio_stream));

  VideoStream video_stream =
      CreateStream(1, video_config, use_android_rtp_hack);
  video_stream.codec = VideoCodec::kNotSpecified;
  video_stream.stream.rtp_payload_type =
      GetPayloadType(VideoCodec::kNotSpecified, use_android_rtp_hack);
  offer.video_streams.push_back(std::move(video_stream));

  return offer;
}

bool IsValidAudioCaptureConfig(const AudioCaptureConfig& config) {
  return config.channels >= 1 && config.bit_rate >= 0;
}

// We don't support resolutions below our minimums.
bool IsSupportedResolution(const Resolution& resolution) {
  return resolution.width > kMinVideoWidth &&
         resolution.height > kMinVideoHeight;
}

bool IsValidVideoCaptureConfig(const VideoCaptureConfig& config) {
  return config.max_frame_rate.is_positive() &&
         ((config.max_bit_rate == 0) ||
          (config.max_bit_rate >= kDefaultVideoMinBitRate)) &&
         !config.resolutions.empty() &&
         std::all_of(config.resolutions.begin(), config.resolutions.end(),
                     IsSupportedResolution);
}

bool AreAllValid(const std::vector<AudioCaptureConfig>& audio_configs,
                 const std::vector<VideoCaptureConfig>& video_configs) {
  return std::all_of(audio_configs.begin(), audio_configs.end(),
                     IsValidAudioCaptureConfig) &&
         std::all_of(video_configs.begin(), video_configs.end(),
                     IsValidVideoCaptureConfig);
}

RemotingCapabilities ToCapabilities(const ReceiverCapability& capability) {
  RemotingCapabilities out;
  for (MediaCapability c : capability.media_capabilities) {
    switch (c) {
      case MediaCapability::kAudio:
        out.audio.push_back(AudioCapability::kBaselineSet);
        break;
      case MediaCapability::kAac:
        out.audio.push_back(AudioCapability::kAac);
        break;
      case MediaCapability::kOpus:
        out.audio.push_back(AudioCapability::kOpus);
        break;
      case MediaCapability::k4k:
        out.video.push_back(VideoCapability::kSupports4k);
        break;
      case MediaCapability::kH264:
        out.video.push_back(VideoCapability::kH264);
        break;
      case MediaCapability::kVp8:
        out.video.push_back(VideoCapability::kVp8);
        break;
      case MediaCapability::kVp9:
        out.video.push_back(VideoCapability::kVp9);
        break;
      case MediaCapability::kHevc:
        out.video.push_back(VideoCapability::kHevc);
        break;
      case MediaCapability::kAv1:
        out.video.push_back(VideoCapability::kAv1);
        break;
      case MediaCapability::kVideo:
        // noop, as "video" is ignored by Chrome remoting.
        break;

      default:
        OSP_NOTREACHED();
    }
  }
  return out;
}

}  // namespace

SenderSession::Client::~Client() = default;

SenderSession::SenderSession(Configuration config)
    : config_(config),
      messenger_(
          config_.message_port,
          config_.message_source_id,
          config_.message_destination_id,
          [this](Error error) {
            OSP_DLOG_WARN << "SenderSession message port error: " << error;
            config_.client->OnError(this, error);
          },
          config_.environment->task_runner()),
      rpc_messenger_([this](std::vector<uint8_t> message) {
        SendRpcMessage(std::move(message));
      }),
      packet_router_(config_.environment) {
  OSP_DCHECK(config_.client);
  OSP_DCHECK(config_.environment);

  // We may or may not do remoting this session, however our RPC handler
  // is not negotiation-specific and registering on construction here allows us
  // to record any unexpected RPC messages.
  messenger_.SetHandler(ReceiverMessage::Type::kRpc,
                        [this](ReceiverMessage message) {
                          this->OnRpcMessage(std::move(message));
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

  Offer offer = CreateMirroringOffer(audio_configs, video_configs,
                                     config_.use_android_rtp_hack);
  return StartNegotiation(std::move(audio_configs), std::move(video_configs),
                          std::move(offer));
}

Error SenderSession::NegotiateRemoting(AudioCaptureConfig audio_config,
                                       VideoCaptureConfig video_config) {
  // Remoting requires both an audio and a video configuration.
  if (!IsValidAudioCaptureConfig(audio_config) ||
      !IsValidVideoCaptureConfig(video_config)) {
    return Error(Error::Code::kParameterInvalid,
                 "Passed invalid audio or video config.");
  }

  Offer offer = CreateRemotingOffer(audio_config, video_config,
                                    config_.use_android_rtp_hack);
  return StartNegotiation({audio_config}, {video_config}, std::move(offer));
}

int SenderSession::GetEstimatedNetworkBandwidth() const {
  return packet_router_.ComputeNetworkBandwidth();
}

void SenderSession::ResetState() {
  state_ = State::kIdle;
  current_negotiation_.reset();
  current_audio_sender_.reset();
  current_video_sender_.reset();
}

Error SenderSession::StartNegotiation(
    std::vector<AudioCaptureConfig> audio_configs,
    std::vector<VideoCaptureConfig> video_configs,
    Offer offer) {
  current_negotiation_ =
      std::unique_ptr<InProcessNegotiation>(new InProcessNegotiation{
          offer, std::move(audio_configs), std::move(video_configs)});

  return messenger_.SendRequest(
      SenderMessage{SenderMessage::Type::kOffer, ++current_sequence_number_,
                    true, std::move(offer)},
      ReceiverMessage::Type::kAnswer,
      [this](ReceiverMessage message) { OnAnswer(message); });
}

void SenderSession::OnAnswer(ReceiverMessage message) {
  if (!message.valid) {
    HandleErrorMessage(message, "Invalid answer response message");
    return;
  }

  // There isn't an obvious way to tell from the Answer whether it is mirroring
  // or remoting specific--the only clues are in the original offer message.
  const Answer& answer = absl::get<Answer>(message.body);
  if (current_negotiation_->offer.cast_mode == CastMode::kMirroring) {
    ConfiguredSenders senders = SpawnSenders(answer);
    // If we didn't select any senders, the negotiation was unsuccessful.
    if (senders.audio_sender == nullptr && senders.video_sender == nullptr) {
      return;
    }

    state_ = State::kStreaming;
    config_.client->OnNegotiated(
        this, std::move(senders),
        capture_recommendations::GetRecommendations(answer));
  } else {
    state_ = State::kRemoting;

    // We don't want to spawn senders yet, since we don't know what the
    // receiver's capabilities are. So, we cache the Answer until the
    // capabilites request is completed.
    current_negotiation_->answer = answer;
    const Error result = messenger_.SendRequest(
        SenderMessage{SenderMessage::Type::kGetCapabilities,
                      ++current_sequence_number_, true},
        ReceiverMessage::Type::kCapabilitiesResponse,
        [this](ReceiverMessage message) { OnCapabilitiesResponse(message); });
    if (!result.ok()) {
      config_.client->OnError(
          this, Error(Error::Code::kNegotiationFailure,
                      "Failed to set a GET_CAPABILITIES request"));
    }
  }
}

void SenderSession::OnCapabilitiesResponse(ReceiverMessage message) {
  if (!current_negotiation_ || !current_negotiation_->answer.IsValid()) {
    OSP_LOG_INFO
        << "Received a capabilities response, but not negotiating anything.";
    return;
  }

  if (!message.valid) {
    HandleErrorMessage(
        message,
        "Bad CAPABILITIES_RESPONSE, assuming remoting is not supported");
    return;
  }

  const ReceiverCapability& caps = absl::get<ReceiverCapability>(message.body);
  int remoting_version = caps.remoting_version;
  // If not set, we assume it is version 1.
  if (remoting_version == ReceiverCapability::kRemotingVersionUnknown) {
    remoting_version = 1;
  }

  if (remoting_version > kSupportedRemotingVersion) {
    std::string message = StringPrintf(
        "Receiver is using too new of a version for remoting (%d > %d)",
        remoting_version, kSupportedRemotingVersion);
    config_.client->OnError(
        this, Error(Error::Code::kRemotingNotSupported, std::move(message)));
    return;
  }

  ConfiguredSenders senders = SpawnSenders(current_negotiation_->answer);
  // If we didn't select any senders, the negotiation was unsuccessful.
  if (senders.audio_sender == nullptr && senders.video_sender == nullptr) {
    config_.client->OnError(this,
                            Error(Error::Code::kNegotiationFailure,
                                  "Failed to negotiate a remoting session."));
    return;
  }

  config_.client->OnRemotingNegotiated(
      this, RemotingNegotiation{std::move(senders), ToCapabilities(caps)});
}

void SenderSession::OnRpcMessage(ReceiverMessage message) {
  if (!message.valid) {
    HandleErrorMessage(
        message,
        "Bad RPC message. This may or may not represent a serious problem");
    return;
  }

  const auto& body = absl::get<std::vector<uint8_t>>(message.body);
  rpc_messenger_.ProcessMessageFromRemote(body.data(), body.size());
}

void SenderSession::HandleErrorMessage(ReceiverMessage message,
                                       const std::string& text) {
  OSP_DCHECK(!message.valid);
  if (absl::holds_alternative<ReceiverError>(message.body)) {
    const ReceiverError& error = absl::get<ReceiverError>(message.body);
    std::string error_text =
        StringPrintf("%s. Error code: %d, description: %s", text.c_str(),
                     error.code, error.description.c_str());
    config_.client->OnError(
        this, Error(Error::Code::kParameterInvalid, std::move(error_text)));
  } else {
    config_.client->OnError(this, Error(Error::Code::kJsonParseError, text));
  }
}

std::unique_ptr<Sender> SenderSession::CreateSender(Ssrc receiver_ssrc,
                                                    const Stream& stream,
                                                    RtpPayloadType type) {
  // Session config is currently only for mirroring.
  SessionConfig config{stream.ssrc,
                       receiver_ssrc,
                       stream.rtp_timebase,
                       stream.channels,
                       stream.target_delay,
                       stream.aes_key,
                       stream.aes_iv_mask,
                       /* is_pli_enabled*/ true};

  return std::make_unique<Sender>(config_.environment, &packet_router_,
                                  std::move(config), type);
}

void SenderSession::SpawnAudioSender(ConfiguredSenders* senders,
                                     Ssrc receiver_ssrc,
                                     int send_index,
                                     int config_index) {
  const AudioCaptureConfig& config =
      current_negotiation_->audio_configs[config_index];
  const RtpPayloadType payload_type =
      GetPayloadType(config.codec, config_.use_android_rtp_hack);
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
  const RtpPayloadType payload_type =
      GetPayloadType(config.codec, config_.use_android_rtp_hack);
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
  // address of the receiver, we don't know where to send the separate UDP
  // stream until we get the ANSWER message here.
  config_.environment->set_remote_endpoint(IPEndpoint{
      config_.remote_address, static_cast<uint16_t>(answer.udp_port)});
  OSP_LOG_INFO << "Streaming to " << config_.environment->remote_endpoint()
               << "...";

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

void SenderSession::SendRpcMessage(std::vector<uint8_t> message_body) {
  Error error = this->messenger_.SendOutboundMessage(SenderMessage{
      SenderMessage::Type::kRpc, ++(this->current_sequence_number_), true,
      std::move(message_body)});

  if (!error.ok()) {
    OSP_LOG_WARN << "Failed to send RPC message: " << error;
  }
}

}  // namespace cast
}  // namespace openscreen
