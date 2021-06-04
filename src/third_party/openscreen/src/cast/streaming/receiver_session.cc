// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/public/message_port.h"
#include "cast/streaming/answer_messages.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/receiver.h"
#include "cast/streaming/sender_message.h"
#include "util/json/json_helpers.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

template <typename Stream, typename Codec>
std::unique_ptr<Stream> SelectStream(
    const std::vector<Codec>& preferred_codecs,
    const std::vector<Stream>& offered_streams) {
  for (auto codec : preferred_codecs) {
    for (const Stream& offered_stream : offered_streams) {
      if (offered_stream.codec == codec) {
        OSP_DVLOG << "Selected " << CodecToString(codec)
                  << " as codec for streaming";
        return std::make_unique<Stream>(offered_stream);
      }
    }
  }
  return nullptr;
}

MediaCapability ToCapability(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAac:
      return MediaCapability::kAac;
    case AudioCodec::kOpus:
      return MediaCapability::kOpus;
    default:
      OSP_DLOG_FATAL << "Invalid audio codec: " << static_cast<int>(codec);
      OSP_NOTREACHED();
  }
}

MediaCapability ToCapability(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kVp8:
      return MediaCapability::kVp8;
    case VideoCodec::kVp9:
      return MediaCapability::kVp9;
    case VideoCodec::kH264:
      return MediaCapability::kH264;
    case VideoCodec::kHevc:
      return MediaCapability::kHevc;
    default:
      OSP_DLOG_FATAL << "Invalid video codec: " << static_cast<int>(codec);
      OSP_NOTREACHED();
  }
}

}  // namespace

ReceiverSession::Client::~Client() = default;

using Preferences = ReceiverSession::Preferences;

Preferences::Preferences() = default;
Preferences::Preferences(std::vector<VideoCodec> video_codecs,
                         std::vector<AudioCodec> audio_codecs)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)) {}

Preferences::Preferences(std::vector<VideoCodec> video_codecs,
                         std::vector<AudioCodec> audio_codecs,
                         std::vector<AudioLimits> audio_limits,
                         std::vector<VideoLimits> video_limits,
                         std::unique_ptr<Display> description)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)),
      audio_limits(std::move(audio_limits)),
      video_limits(std::move(video_limits)),
      display_description(std::move(description)) {}

Preferences::Preferences(Preferences&&) noexcept = default;
Preferences& Preferences::operator=(Preferences&&) noexcept = default;

ReceiverSession::ReceiverSession(Client* const client,
                                 Environment* environment,
                                 MessagePort* message_port,
                                 Preferences preferences)
    : client_(client),
      environment_(environment),
      preferences_(std::move(preferences)),
      session_id_(MakeUniqueSessionId("streaming_receiver")),
      messager_(message_port,
                session_id_,
                [this](Error error) {
                  OSP_DLOG_WARN << "Got a session messager error: " << error;
                  client_->OnError(this, error);
                }),
      packet_router_(environment_) {
  OSP_DCHECK(client_);
  OSP_DCHECK(environment_);

  messager_.SetHandler(
      SenderMessage::Type::kOffer,
      [this](SenderMessage message) { OnOffer(std::move(message)); });
  messager_.SetHandler(SenderMessage::Type::kGetCapabilities,
                       [this](SenderMessage message) {
                         OnCapabilitiesRequest(std::move(message));
                       });
  environment_->SetSocketSubscriber(this);
}

ReceiverSession::~ReceiverSession() {
  ResetReceivers(Client::kEndOfSession);
}

void ReceiverSession::OnSocketReady() {
  if (pending_session_) {
    InitializeSession(*pending_session_);
    pending_session_.reset();
  }
}

void ReceiverSession::OnSocketInvalid(Error error) {
  if (pending_session_) {
    SendErrorAnswerReply(pending_session_->sequence_number,
                         "Failed to bind UDP socket");
    pending_session_.reset();
  }

  client_->OnError(this,
                   Error(Error::Code::kSocketFailure,
                         "The environment is invalid and should be replaced."));
}

bool ReceiverSession::SessionProperties::IsValid() const {
  return (selected_audio || selected_video) && sequence_number >= 0;
}

void ReceiverSession::OnOffer(SenderMessage message) {
  // We just drop offers we can't respond to. Note that libcast senders will
  // always send a strictly positive sequence numbers, but zero is permitted
  // by the spec.
  if (message.sequence_number < 0) {
    OSP_DLOG_WARN
        << "Dropping offer with missing sequence number, can't respond";
    return;
  }

  if (!message.valid) {
    SendErrorAnswerReply(message.sequence_number,
                         "Failed to parse malformed OFFER");
    client_->OnError(this, Error(Error::Code::kParameterInvalid,
                                 "Received invalid OFFER message"));
    return;
  }

  auto properties = std::make_unique<SessionProperties>();
  properties->sequence_number = message.sequence_number;

  // TODO(issuetracker.google.com/184186390): ReceiverSession needs to support
  // fielding remoting offers.
  const Offer& offer = absl::get<Offer>(message.body);
  if (offer.cast_mode == CastMode::kRemoting) {
    SendErrorAnswerReply(message.sequence_number,
                         "Remoting support is not complete in libcast");
    return;
  }

  if (!offer.audio_streams.empty() && !preferences_.audio_codecs.empty()) {
    properties->selected_audio =
        SelectStream(preferences_.audio_codecs, offer.audio_streams);
  }

  if (!offer.video_streams.empty() && !preferences_.video_codecs.empty()) {
    properties->selected_video =
        SelectStream(preferences_.video_codecs, offer.video_streams);
  }

  if (!properties->IsValid()) {
    SendErrorAnswerReply(message.sequence_number,
                         "Failed to select any streams from OFFER");
    return;
  }

  switch (environment_->socket_state()) {
    // If the environment is ready or in a bad state, we can respond
    // immediately.
    case Environment::SocketState::kInvalid:
      SendErrorAnswerReply(message.sequence_number,
                           "UDP socket is closed, likely due to a bind error.");
      break;

    case Environment::SocketState::kReady:
      InitializeSession(*properties);
      break;

    // Else we need to store the properties we just created until we get a
    // ready or error event.
    case Environment::SocketState::kStarting:
      pending_session_ = std::move(properties);
      break;
  }
}

void ReceiverSession::OnCapabilitiesRequest(SenderMessage message) {
  if (message.sequence_number < 0) {
    OSP_DLOG_WARN
        << "Dropping offer with missing sequence number, can't respond";
    return;
  }

  ReceiverMessage response{
      ReceiverMessage::Type::kCapabilitiesResponse, message.sequence_number,
      true /* valid */
  };
  if (preferences_.remoting) {
    response.body = CreateRemotingCapabilityV2();
  } else {
    response.valid = false;
    response.body =
        ReceiverError{static_cast<int>(Error::Code::kRemotingNotSupported),
                      "Remoting is not supported"};
  }

  const Error result = messager_.SendMessage(std::move(response));
  if (!result.ok()) {
    client_->OnError(this, std::move(result));
  }
}

void ReceiverSession::InitializeSession(const SessionProperties& properties) {
  Answer answer = ConstructAnswer(properties);
  if (!answer.IsValid()) {
    // If the answer message is invalid, there is no point in setting up a
    // negotiation because the sender won't be able to connect to it.
    SendErrorAnswerReply(properties.sequence_number,
                         "Failed to construct an ANSWER message");
    return;
  }

  // Only spawn receivers if we know we have a valid answer message.
  ConfiguredReceivers receivers = SpawnReceivers(properties);
  client_->OnNegotiated(this, std::move(receivers));
  const Error result = messager_.SendMessage(ReceiverMessage{
      ReceiverMessage::Type::kAnswer, properties.sequence_number,
      true /* valid */, std::move(answer)});
  if (!result.ok()) {
    client_->OnError(this, std::move(result));
  }
}

std::unique_ptr<Receiver> ReceiverSession::ConstructReceiver(
    const Stream& stream) {
  // Session config is currently only for mirroring.
  SessionConfig config = {stream.ssrc,         stream.ssrc + 1,
                          stream.rtp_timebase, stream.channels,
                          stream.target_delay, stream.aes_key,
                          stream.aes_iv_mask,  /* is_pli_enabled */ true};
  return std::make_unique<Receiver>(environment_, &packet_router_,
                                    std::move(config));
}

ReceiverSession::ConfiguredReceivers ReceiverSession::SpawnReceivers(
    const SessionProperties& properties) {
  OSP_DCHECK(properties.IsValid());
  ResetReceivers(Client::kRenegotiated);

  AudioCaptureConfig audio_config;
  if (properties.selected_audio) {
    current_audio_receiver_ =
        ConstructReceiver(properties.selected_audio->stream);
    audio_config =
        AudioCaptureConfig{properties.selected_audio->codec,
                           properties.selected_audio->stream.channels,
                           properties.selected_audio->bit_rate,
                           properties.selected_audio->stream.rtp_timebase,
                           properties.selected_audio->stream.target_delay};
  }

  VideoCaptureConfig video_config;
  if (properties.selected_video) {
    current_video_receiver_ =
        ConstructReceiver(properties.selected_video->stream);
    video_config =
        VideoCaptureConfig{properties.selected_video->codec,
                           properties.selected_video->max_frame_rate,
                           properties.selected_video->max_bit_rate,
                           properties.selected_video->resolutions,
                           properties.selected_video->stream.target_delay};
  }

  return ConfiguredReceivers{
      current_audio_receiver_.get(), std::move(audio_config),
      current_video_receiver_.get(), std::move(video_config)};
}

void ReceiverSession::ResetReceivers(Client::ReceiversDestroyingReason reason) {
  if (current_video_receiver_ || current_audio_receiver_) {
    client_->OnReceiversDestroying(this, reason);
    current_audio_receiver_.reset();
    current_video_receiver_.reset();
  }
}

Answer ReceiverSession::ConstructAnswer(const SessionProperties& properties) {
  OSP_DCHECK(properties.IsValid());

  std::vector<int> stream_indexes;
  std::vector<Ssrc> stream_ssrcs;
  Constraints constraints;
  if (properties.selected_audio) {
    stream_indexes.push_back(properties.selected_audio->stream.index);
    stream_ssrcs.push_back(properties.selected_audio->stream.ssrc + 1);

    for (const auto& limit : preferences_.audio_limits) {
      if (limit.codec == properties.selected_audio->codec ||
          limit.applies_to_all_codecs) {
        constraints.audio = AudioConstraints{
            limit.max_sample_rate, limit.max_channels, limit.min_bit_rate,
            limit.max_bit_rate,    limit.max_delay,
        };
        break;
      }
    }
  }

  if (properties.selected_video) {
    stream_indexes.push_back(properties.selected_video->stream.index);
    stream_ssrcs.push_back(properties.selected_video->stream.ssrc + 1);

    for (const auto& limit : preferences_.video_limits) {
      if (limit.codec == properties.selected_video->codec ||
          limit.applies_to_all_codecs) {
        constraints.video = VideoConstraints{
            limit.max_pixels_per_second, absl::nullopt, /* min dimensions */
            limit.max_dimensions,        limit.min_bit_rate,
            limit.max_bit_rate,          limit.max_delay,
        };
        break;
      }
    }
  }

  absl::optional<DisplayDescription> display;
  if (preferences_.display_description) {
    const auto* d = preferences_.display_description.get();
    display = DisplayDescription{d->dimensions, absl::nullopt,
                                 d->can_scale_content
                                     ? AspectRatioConstraint::kVariable
                                     : AspectRatioConstraint::kFixed};
  }

  // Only set the constraints in the answer if they are valid (meaning we
  // successfully found limits above).
  absl::optional<Constraints> answer_constraints;
  if (constraints.IsValid()) {
    answer_constraints = std::move(constraints);
  }
  return Answer{environment_->GetBoundLocalEndpoint().port,
                std::move(stream_indexes), std::move(stream_ssrcs),
                answer_constraints, std::move(display)};
}

ReceiverCapability ReceiverSession::CreateRemotingCapabilityV2() {
  // If we don't support remoting, there is no reason to respond to
  // capability requests--they are not used for mirroring.
  OSP_DCHECK(preferences_.remoting);
  ReceiverCapability capability;
  capability.remoting_version = kSupportedRemotingVersion;

  for (const AudioCodec& codec : preferences_.audio_codecs) {
    capability.media_capabilities.push_back(ToCapability(codec));
  }
  for (const VideoCodec& codec : preferences_.video_codecs) {
    capability.media_capabilities.push_back(ToCapability(codec));
  }

  if (preferences_.remoting->supports_chrome_audio_codecs) {
    capability.media_capabilities.push_back(MediaCapability::kAudio);
  }
  if (preferences_.remoting->supports_4k) {
    capability.media_capabilities.push_back(MediaCapability::k4k);
  }
  return capability;
}

void ReceiverSession::SendErrorAnswerReply(int sequence_number,
                                           const char* message) {
  const Error error(Error::Code::kParseError, message);
  OSP_DLOG_WARN << message;
  const Error result = messager_.SendMessage(ReceiverMessage{
      ReceiverMessage::Type::kAnswer, sequence_number, false /* valid */,
      ReceiverError{static_cast<int>(Error::Code::kParseError), message}});
  if (!result.ok()) {
    client_->OnError(this, std::move(result));
  }
}

}  // namespace cast
}  // namespace openscreen
