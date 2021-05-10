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
#include "cast/streaming/environment.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/receiver.h"
#include "cast/streaming/sender_message.h"
#include "util/json/json_helpers.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

// Using statements for constructor readability.
using Preferences = ReceiverSession::Preferences;
using ConfiguredReceivers = ReceiverSession::ConfiguredReceivers;

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

DisplayResolution ToDisplayResolution(const Resolution& resolution) {
  return DisplayResolution{resolution.width, resolution.height};
}

}  // namespace

ReceiverSession::Client::~Client() = default;

Preferences::Preferences() = default;
Preferences::Preferences(std::vector<VideoCodec> video_codecs,
                         std::vector<AudioCodec> audio_codecs)
    : Preferences(video_codecs, audio_codecs, nullptr, nullptr) {}

Preferences::Preferences(std::vector<VideoCodec> video_codecs,
                         std::vector<AudioCodec> audio_codecs,
                         std::unique_ptr<Constraints> constraints,
                         std::unique_ptr<DisplayDescription> description)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)),
      constraints(std::move(constraints)),
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

  const Offer& offer = absl::get<Offer>(message.body);
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

ConfiguredReceivers ReceiverSession::SpawnReceivers(
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
    std::vector<DisplayResolution> display_resolutions;
    std::transform(properties.selected_video->resolutions.begin(),
                   properties.selected_video->resolutions.end(),
                   std::back_inserter(display_resolutions),
                   ToDisplayResolution);
    video_config = VideoCaptureConfig{
        properties.selected_video->codec,
        FrameRate{properties.selected_video->max_frame_rate.numerator,
                  properties.selected_video->max_frame_rate.denominator},
        properties.selected_video->max_bit_rate, std::move(display_resolutions),
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
  if (properties.selected_audio) {
    stream_indexes.push_back(properties.selected_audio->stream.index);
    stream_ssrcs.push_back(properties.selected_audio->stream.ssrc + 1);
  }

  if (properties.selected_video) {
    stream_indexes.push_back(properties.selected_video->stream.index);
    stream_ssrcs.push_back(properties.selected_video->stream.ssrc + 1);
  }

  absl::optional<Constraints> constraints;
  if (preferences_.constraints) {
    constraints = absl::optional<Constraints>(*preferences_.constraints);
  }

  absl::optional<DisplayDescription> display;
  if (preferences_.display_description) {
    display =
        absl::optional<DisplayDescription>(*preferences_.display_description);
  }

  return Answer{environment_->GetBoundLocalEndpoint().port,
                std::move(stream_indexes),
                std::move(stream_ssrcs),
                std::move(constraints),
                std::move(display),
                std::vector<int>{},  // receiver_rtcp_event_log
                std::vector<int>{},  // receiver_rtcp_dscp
                supports_wifi_status_reporting_};
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
