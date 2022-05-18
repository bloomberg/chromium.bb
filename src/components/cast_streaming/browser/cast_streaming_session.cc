// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_streaming_session.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "components/cast_streaming/browser/stream_consumer.h"
#include "components/cast_streaming/public/config_conversions.h"
#include "media/base/demuxer_stream.h"
#include "media/base/timestamp_constants.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace cast_streaming {
namespace {

// Timeout to stop the Session when no data is received.
constexpr base::TimeDelta kNoDataTimeout = base::Seconds(15);

bool CreateDataPipeForStreamType(media::DemuxerStream::Type type,
                                 mojo::ScopedDataPipeProducerHandle* producer,
                                 mojo::ScopedDataPipeConsumerHandle* consumer) {
  const MojoCreateDataPipeOptions data_pipe_options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE,
      1u /* element_num_bytes */,
      media::GetDefaultDecoderBufferConverterCapacity(type)};
  MojoResult result =
      mojo::CreateDataPipe(&data_pipe_options, *producer, *consumer);
  return result == MOJO_RESULT_OK;
}

// Timeout to end the Session when no offer message is sent.
constexpr base::TimeDelta kInitTimeout = base::Seconds(5);

StreamingInitializationInfo CreateMirroringInitializationInfo(
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) {
  absl::optional<StreamingInitializationInfo::AudioStreamInfo>
      audio_stream_info;
  if (receivers.audio_receiver) {
    audio_stream_info.emplace(ToAudioDecoderConfig(receivers.audio_config),
                              receivers.audio_receiver);
  }

  absl::optional<StreamingInitializationInfo::VideoStreamInfo>
      video_stream_info;
  if (receivers.video_receiver) {
    video_stream_info.emplace(ToVideoDecoderConfig(receivers.video_config),
                              receivers.video_receiver);
  }

  return {session, std::move(audio_stream_info), std::move(video_stream_info)};
}

}  // namespace

CastStreamingSession::ReceiverSessionClient::ReceiverSessionClient(
    CastStreamingSession::Client* client,
    absl::optional<RendererControllerConfig> renderer_controls,
    std::unique_ptr<ReceiverSession::AVConstraints> av_constraints,
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner),
      environment_(&openscreen::Clock::now, &task_runner_),
      cast_message_port_impl_(
          std::move(message_port),
          base::BindOnce(
              &CastStreamingSession::ReceiverSessionClient::OnCastChannelClosed,
              base::Unretained(this))),
      client_(client),
      weak_factory_(this) {
  DCHECK(task_runner);
  DCHECK(client_);

  receiver_session_ = std::make_unique<openscreen::cast::ReceiverSession>(
      this, &environment_, &cast_message_port_impl_,
      std::move(*av_constraints));

  if (renderer_controls) {
    playback_command_dispatcher_ = std::make_unique<PlaybackCommandDispatcher>(
        task_runner,
        std::move(renderer_controls.value().control_configuration));
    playback_command_dispatcher_->RegisterCommandSource(
        std::move(renderer_controls.value().external_renderer_controls));
  }

  init_timeout_timer_.Start(
      FROM_HERE, kInitTimeout,
      base::BindOnce(
          &CastStreamingSession::ReceiverSessionClient::OnInitializationTimeout,
          base::Unretained(this)));
}

void CastStreamingSession::ReceiverSessionClient::GetAudioBuffer(
    base::OnceClosure no_frames_available_cb) {
  DCHECK(audio_consumer_);
  audio_consumer_->ReadFrame(std::move(no_frames_available_cb));
}

void CastStreamingSession::ReceiverSessionClient::GetVideoBuffer(
    base::OnceClosure no_frames_available_cb) {
  DCHECK(video_consumer_);
  video_consumer_->ReadFrame(std::move(no_frames_available_cb));
}

CastStreamingSession::ReceiverSessionClient::~ReceiverSessionClient() = default;

void CastStreamingSession::ReceiverSessionClient::OnInitializationTimeout() {
  DVLOG(1) << __func__;
  DCHECK(!is_initialized_);
  client_->OnSessionEnded();
  is_initialized_ = true;
}

absl::optional<mojo::ScopedDataPipeConsumerHandle>
CastStreamingSession::ReceiverSessionClient::InitializeAudioConsumer(
    const StreamingInitializationInfo& initialization_info) {
  if (!initialization_info.audio_stream_info) {
    return absl::nullopt;
  }

  // Create the audio data pipe.
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  if (!CreateDataPipeForStreamType(media::DemuxerStream::Type::AUDIO,
                                   &data_pipe_producer, &data_pipe_consumer)) {
    return absl::nullopt;
  }

  if (!audio_consumer_) {
    // We can use unretained pointers here because StreamConsumer is owned by
    // this object and |client_| is guaranteed to outlive this object. Here,
    // the duration is set to kNoTimestamp so the audio renderer does not block.
    // Audio frames duration is not known ahead of time in mirroring.
    audio_consumer_ = std::make_unique<StreamConsumer>(
        initialization_info.audio_stream_info->receiver, media::kNoTimestamp,
        std::move(data_pipe_producer),
        base::BindRepeating(
            &CastStreamingSession::Client::OnAudioBufferReceived,
            base::Unretained(client_)),
        base::BindRepeating(&base::OneShotTimer::Reset,
                            base::Unretained(&data_timeout_timer_)));
  } else {
    audio_consumer_ = std::make_unique<StreamConsumer>(
        std::move(*audio_consumer_),
        initialization_info.audio_stream_info->receiver,
        std::move(data_pipe_producer));
  }

  return data_pipe_consumer;
}

absl::optional<mojo::ScopedDataPipeConsumerHandle>
CastStreamingSession::ReceiverSessionClient::InitializeVideoConsumer(
    const StreamingInitializationInfo& initialization_info) {
  if (!initialization_info.video_stream_info) {
    return absl::nullopt;
  }

  // Create the video data pipe.
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  if (!CreateDataPipeForStreamType(media::DemuxerStream::Type::VIDEO,
                                   &data_pipe_producer, &data_pipe_consumer)) {
    return absl::nullopt;
  }

  if (!video_consumer_) {
    // We can use unretained pointers here because StreamConsumer is owned by
    // this object and |client_| is guaranteed to outlive this object.
    // |data_timeout_timer_| is also owned by this object and will outlive both
    // StreamConsumers.
    // The frame duration is set to 10 minutes to work around cases where
    // senders do not send data for a long period of time. We end up with
    // overlapping video frames but this is fine since the media pipeline mostly
    // considers the playout time when deciding which frame to present or play
    video_consumer_ = std::make_unique<StreamConsumer>(
        initialization_info.video_stream_info->receiver, base::Minutes(10),
        std::move(data_pipe_producer),
        base::BindRepeating(
            &CastStreamingSession::Client::OnVideoBufferReceived,
            base::Unretained(client_)),
        base::BindRepeating(&base::OneShotTimer::Reset,
                            base::Unretained(&data_timeout_timer_)));
  } else {
    video_consumer_ = std::make_unique<StreamConsumer>(
        std::move(*video_consumer_),
        initialization_info.video_stream_info->receiver,
        std::move(data_pipe_producer));
  }

  return data_pipe_consumer;
}

void CastStreamingSession::ReceiverSessionClient::StartStreamingSession(
    StreamingInitializationInfo initialization_info) {
  DVLOG(1) << __func__;
  DCHECK_EQ(initialization_info.session, receiver_session_.get());

  // This is necessary in case the offer message had no audio and no video
  // stream.
  if (!initialization_info.audio_stream_info &&
      !initialization_info.video_stream_info) {
    client_->OnSessionEnded();
    return;
  }

  init_timeout_timer_.Stop();

  bool is_new_offer = is_initialized_;
  if (is_new_offer) {
    // This is a second offer message, reinitialize the streams.
    const bool existing_session_has_audio = !!audio_consumer_;
    const bool existing_session_has_video = !!video_consumer_;
    const bool new_offer_has_audio = !!initialization_info.audio_stream_info;
    const bool new_offer_has_video = !!initialization_info.video_stream_info;

    if (new_offer_has_audio != existing_session_has_audio ||
        new_offer_has_video != existing_session_has_video) {
      // This call to StartStreamingSession() has support for audio and/or video
      // streaming which does not match the ones provided during a prior call to
      // this method. Return early here.
      DLOG(ERROR) << "New streaming session has support for audio or video "
                     "which does not match the ones provided during a prior "
                     "streaming initialization.";
      client_->OnSessionEnded();
      return;
    }
  }

  // Set |is_initialized_| now so we can return early on failure.
  is_initialized_ = true;

  absl::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer_handle;
  if (initialization_info.audio_stream_info) {
    audio_pipe_consumer_handle = InitializeAudioConsumer(initialization_info);
    if (audio_pipe_consumer_handle) {
      DVLOG(1) << "Initialized audio stream. "
               << initialization_info.audio_stream_info->config
                      .AsHumanReadableString();
    } else {
      client_->OnSessionEnded();
      return;
    }
  }

  absl::optional<mojo::ScopedDataPipeConsumerHandle> video_pipe_consumer_handle;
  if (initialization_info.video_stream_info) {
    video_pipe_consumer_handle = InitializeVideoConsumer(initialization_info);
    if (video_pipe_consumer_handle) {
      DVLOG(1) << "Initialized video stream. "
               << initialization_info.video_stream_info->config
                      .AsHumanReadableString();
    } else {
      audio_consumer_.reset();
      client_->OnSessionEnded();
      return;
    }
  }

  if (is_new_offer) {
    client_->OnSessionReinitialization(std::move(initialization_info),
                                       std::move(audio_pipe_consumer_handle),
                                       std::move(video_pipe_consumer_handle));
  } else {
    client_->OnSessionInitialization(std::move(initialization_info),
                                     std::move(audio_pipe_consumer_handle),
                                     std::move(video_pipe_consumer_handle));
    data_timeout_timer_.Start(
        FROM_HERE, kNoDataTimeout,
        base::BindOnce(
            &CastStreamingSession::ReceiverSessionClient::OnDataTimeout,
            base::Unretained(this)));
  }
}

void CastStreamingSession::ReceiverSessionClient::OnNegotiated(
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) {
  StartStreamingSession(
      CreateMirroringInitializationInfo(session, std::move(receivers)));
}

void CastStreamingSession::ReceiverSessionClient::OnRemotingNegotiated(
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::RemotingNegotiation negotiation) {
  DCHECK(playback_command_dispatcher_);
  playback_command_dispatcher_->OnRemotingSessionNegotiated(
      negotiation.messenger);
  playback_command_dispatcher_->ConfigureRemotingAsync(
      this, session, std::move(negotiation.receivers));
}

void CastStreamingSession::ReceiverSessionClient::OnReceiversDestroying(
    const openscreen::cast::ReceiverSession* session,
    ReceiversDestroyingReason reason) {
  // This can be called when |receiver_session_| is being destroyed, so we
  // do not sanity-check |session| here.
  DVLOG(1) << __func__;
  if (playback_command_dispatcher_) {
    playback_command_dispatcher_->OnRemotingSessionEnded();
  }

  switch (reason) {
    case ReceiversDestroyingReason::kEndOfSession:
      audio_consumer_.reset();
      video_consumer_.reset();
      client_->OnSessionEnded();
      break;
    case ReceiversDestroyingReason::kRenegotiated:
      break;
  }
}

void CastStreamingSession::ReceiverSessionClient::OnError(
    const openscreen::cast::ReceiverSession* session,
    openscreen::Error error) {
  DCHECK_EQ(session, receiver_session_.get());
  LOG(ERROR) << error;
  if (!is_initialized_) {
    client_->OnSessionEnded();
    is_initialized_ = true;
  }
}

void CastStreamingSession::ReceiverSessionClient::OnDataTimeout() {
  DLOG(ERROR) << __func__ << ": Session ended due to timeout";
  receiver_session_.reset();
}

void CastStreamingSession::ReceiverSessionClient::OnCastChannelClosed() {
  DLOG(ERROR) << __func__ << ": Session ended due to cast channel closure";
  receiver_session_.reset();
}

base::WeakPtr<CastStreamingSession::ReceiverSessionClient>
CastStreamingSession::ReceiverSessionClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

CastStreamingSession::Client::~Client() = default;
CastStreamingSession::CastStreamingSession() = default;
CastStreamingSession::~CastStreamingSession() = default;

void CastStreamingSession::Start(
    Client* client,
    absl::optional<RendererControllerConfig> renderer_controls,
    std::unique_ptr<ReceiverSession::AVConstraints> av_constraints,
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DVLOG(1) << __func__;
  DCHECK(client);
  DCHECK(!receiver_session_);
  receiver_session_ = std::make_unique<ReceiverSessionClient>(
      client, std::move(renderer_controls), std::move(av_constraints),
      std::move(message_port), task_runner);
}

void CastStreamingSession::Stop() {
  DVLOG(1) << __func__;
  DCHECK(receiver_session_);
  receiver_session_.reset();
}

AudioDemuxerStreamDataProvider::RequestBufferCB
CastStreamingSession::GetAudioBufferRequester() {
  DCHECK(receiver_session_);
  return base::BindRepeating(
      &CastStreamingSession::ReceiverSessionClient::GetAudioBuffer,
      receiver_session_->GetWeakPtr());
}

VideoDemuxerStreamDataProvider::RequestBufferCB
CastStreamingSession::GetVideoBufferRequester() {
  DCHECK(receiver_session_);
  return base::BindRepeating(
      &CastStreamingSession::ReceiverSessionClient::GetVideoBuffer,
      receiver_session_->GetWeakPtr());
}

}  // namespace cast_streaming
