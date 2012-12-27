// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>

#include "base/message_loop_proxy.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/audio_encoder_opus.h"
#include "remoting/codec/audio_encoder_speex.h"
#include "remoting/codec/audio_encoder_verbatim.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/codec/video_encoder_vp8.h"
#include "remoting/host/audio_scheduler.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_environment_factory.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/video_scheduler.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_thread_proxy.h"

namespace remoting {

ClientSession::ClientSession(
    EventHandler* event_handler,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_ptr<protocol::ConnectionToClient> connection,
    DesktopEnvironmentFactory* desktop_environment_factory,
    const base::TimeDelta& max_duration)
    : event_handler_(event_handler),
      connection_(connection.Pass()),
      connection_factory_(connection_.get()),
      desktop_environment_(desktop_environment_factory->Create()),
      client_jid_(connection_->session()->jid()),
      host_clipboard_stub_(desktop_environment_->event_executor()),
      host_input_stub_(desktop_environment_->event_executor()),
      input_tracker_(host_input_stub_),
      remote_input_filter_(&input_tracker_),
      mouse_clamping_filter_(desktop_environment_->video_capturer(),
                             &remote_input_filter_),
      disable_input_filter_(&mouse_clamping_filter_),
      disable_clipboard_filter_(clipboard_echo_filter_.host_filter()),
      auth_input_filter_(&disable_input_filter_),
      auth_clipboard_filter_(&disable_clipboard_filter_),
      client_clipboard_factory_(clipboard_echo_filter_.client_filter()),
      max_duration_(max_duration),
      audio_task_runner_(audio_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      video_encode_task_runner_(video_encode_task_runner),
      network_task_runner_(network_task_runner),
      active_recorders_(0) {
  connection_->SetEventHandler(this);

  // TODO(sergeyu): Currently ConnectionToClient expects stubs to be
  // set before channels are connected. Make it possible to set stubs
  // later and set them only when connection is authenticated.
  connection_->set_clipboard_stub(&auth_clipboard_filter_);
  connection_->set_host_stub(this);
  connection_->set_input_stub(&auth_input_filter_);
  clipboard_echo_filter_.set_host_stub(host_clipboard_stub_);

  // |auth_*_filter_|'s states reflect whether the session is authenticated.
  auth_input_filter_.set_enabled(false);
  auth_clipboard_filter_.set_enabled(false);
}

void ClientSession::NotifyClientDimensions(
    const protocol::ClientDimensions& dimensions) {
  if (dimensions.has_width() && dimensions.has_height()) {
    VLOG(1) << "Received ClientDimensions (width="
            << dimensions.width() << ", height=" << dimensions.height() << ")";
    event_handler_->OnClientDimensionsChanged(
        this, SkISize::Make(dimensions.width(), dimensions.height()));
  }
}

void ClientSession::ControlVideo(const protocol::VideoControl& video_control) {
  if (video_control.has_enable()) {
    VLOG(1) << "Received VideoControl (enable="
            << video_control.enable() << ")";
    if (video_scheduler_.get()) {
      video_scheduler_->Pause(!video_control.enable());
    }
  }
}

void ClientSession::ControlAudio(const protocol::AudioControl& audio_control) {
  if (audio_control.has_enable()) {
    VLOG(1) << "Received AudioControl (enable="
            << audio_control.enable() << ")";
    if (audio_scheduler_.get()) {
      audio_scheduler_->SetEnabled(audio_control.enable());
    }
  }
}

void ClientSession::OnConnectionAuthenticated(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  auth_input_filter_.set_enabled(true);
  auth_clipboard_filter_.set_enabled(true);

  clipboard_echo_filter_.set_client_stub(connection_->client_stub());

  if (max_duration_ > base::TimeDelta()) {
    // TODO(simonmorris): Let Disconnect() tell the client that the
    // disconnection was caused by the session exceeding its maximum duration.
    max_duration_timer_.Start(FROM_HERE, max_duration_,
                              this, &ClientSession::Disconnect);
  }

  event_handler_->OnSessionAuthenticated(this);
}

void ClientSession::OnConnectionChannelsConnected(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  SetDisableInputs(false);

  // Let the desktop environment notify us of local clipboard changes.
  desktop_environment_->Start(
      CreateClipboardProxy(),
      client_jid(),
      base::Bind(&protocol::ConnectionToClient::Disconnect,
                 connection_factory_.GetWeakPtr()));

  // Create a VideoEncoder based on the session's video channel configuration.
  scoped_ptr<VideoEncoder> video_encoder =
      CreateVideoEncoder(connection_->session()->config());

  // Create a VideoScheduler to pump frames from the capturer to the client.
  video_scheduler_ = VideoScheduler::Create(
      video_capture_task_runner_,
      video_encode_task_runner_,
      network_task_runner_,
      desktop_environment_->video_capturer(),
      video_encoder.Pass(),
      connection_->client_stub(),
      connection_->video_stub());
  ++active_recorders_;

  // Create an AudioScheduler if audio is enabled, to pump audio samples.
  if (connection_->session()->config().is_audio_enabled()) {
    scoped_ptr<AudioEncoder> audio_encoder =
        CreateAudioEncoder(connection_->session()->config());
    audio_scheduler_ = AudioScheduler::Create(
        audio_task_runner_,
        network_task_runner_,
        desktop_environment_->audio_capturer(),
        audio_encoder.Pass(),
        connection_->audio_stub());
    ++active_recorders_;
  }

  // Notify the event handler that all our channels are now connected.
  event_handler_->OnSessionChannelsConnected(this);
}

void ClientSession::OnConnectionClosed(
    protocol::ConnectionToClient* connection,
    protocol::ErrorCode error) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  // Ignore any further callbacks from the DesktopEnvironment.
  connection_factory_.InvalidateWeakPtrs();

  // If the client never authenticated then the session failed.
  if (!auth_input_filter_.enabled())
    event_handler_->OnSessionAuthenticationFailed(this);

  // Block any further input events from the client.
  // TODO(wez): Fix ChromotingHost::OnSessionClosed not to check our
  // is_authenticated(), so that we can disable |auth_*_filter_| here.
  disable_input_filter_.set_enabled(false);
  disable_clipboard_filter_.set_enabled(false);

  // Ensure that any pressed keys or buttons are released.
  input_tracker_.ReleaseAll();

  // Stop components access the client, audio or video stubs, which are no
  // longer valid once ConnectionToClient calls OnConnectionClosed().
  if (audio_scheduler_.get()) {
    audio_scheduler_->Stop(base::Bind(&ClientSession::OnRecorderStopped, this));
    audio_scheduler_ = NULL;
  }
  if (video_scheduler_.get()) {
    video_scheduler_->Stop(base::Bind(&ClientSession::OnRecorderStopped, this));
    video_scheduler_ = NULL;
  }
  client_clipboard_factory_.InvalidateWeakPtrs();

  // Notify the ChromotingHost that this client is disconnected.
  // TODO(sergeyu): Log failure reason?
  event_handler_->OnSessionClosed(this);
}

void ClientSession::OnSequenceNumberUpdated(
    protocol::ConnectionToClient* connection, int64 sequence_number) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  if (video_scheduler_.get())
    video_scheduler_->UpdateSequenceNumber(sequence_number);

  event_handler_->OnSessionSequenceNumber(this, sequence_number);
}

void ClientSession::OnRouteChange(
    protocol::ConnectionToClient* connection,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  event_handler_->OnSessionRouteChange(this, channel_name, route);
}

void ClientSession::Disconnect() {
  DCHECK(CalledOnValidThread());
  DCHECK(connection_.get());

  max_duration_timer_.Stop();

  // This triggers OnConnectionClosed(), and the session may be destroyed
  // as the result, so this call must be the last in this method.
  connection_->Disconnect();
}

void ClientSession::Stop(const base::Closure& stopped_task) {
  DCHECK(CalledOnValidThread());
  DCHECK(stopped_task_.is_null());
  DCHECK(!stopped_task.is_null());
  DCHECK(audio_scheduler_.get() == NULL);
  DCHECK(video_scheduler_.get() == NULL);

  stopped_task_ = stopped_task;

  if (active_recorders_ == 0) {
    // |stopped_task_| may tear down the signalling layer, so tear-down
    // |connection_| before invoking it.
    connection_.reset();
    stopped_task_.Run();
  }
}

void ClientSession::LocalMouseMoved(const SkIPoint& mouse_pos) {
  DCHECK(CalledOnValidThread());
  remote_input_filter_.LocalMouseMoved(mouse_pos);
}

void ClientSession::SetDisableInputs(bool disable_inputs) {
  DCHECK(CalledOnValidThread());

  if (disable_inputs)
    input_tracker_.ReleaseAll();

  disable_input_filter_.set_enabled(!disable_inputs);
  disable_clipboard_filter_.set_enabled(!disable_inputs);
}

ClientSession::~ClientSession() {
  DCHECK_EQ(active_recorders_, 0);
  DCHECK(audio_scheduler_.get() == NULL);
  DCHECK(video_scheduler_.get() == NULL);
}

scoped_ptr<protocol::ClipboardStub> ClientSession::CreateClipboardProxy() {
  DCHECK(CalledOnValidThread());

  return scoped_ptr<protocol::ClipboardStub>(
      new protocol::ClipboardThreadProxy(
          client_clipboard_factory_.GetWeakPtr(),
          base::MessageLoopProxy::current()));
}

void ClientSession::OnRecorderStopped() {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ClientSession::OnRecorderStopped, this));
    return;
  }

  --active_recorders_;
  DCHECK_GE(active_recorders_, 0);

  DCHECK(!stopped_task_.is_null());
  if (active_recorders_ == 0) {
    // |stopped_task_| may result in the signalling layer being torn down, so
    // tear down the ConnectionToClient first.
    connection_.reset();
    stopped_task_.Run();
  }
}

// TODO(sergeyu): Move this to SessionManager?
// static
scoped_ptr<VideoEncoder> ClientSession::CreateVideoEncoder(
    const protocol::SessionConfig& config) {
  const protocol::ChannelConfig& video_config = config.video_config();

  if (video_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return scoped_ptr<VideoEncoder>(new remoting::VideoEncoderVerbatim());
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_VP8) {
    return scoped_ptr<VideoEncoder>(new remoting::VideoEncoderVp8());
  }

  NOTIMPLEMENTED();
  return scoped_ptr<VideoEncoder>(NULL);
}

// static
scoped_ptr<AudioEncoder> ClientSession::CreateAudioEncoder(
    const protocol::SessionConfig& config) {
  const protocol::ChannelConfig& audio_config = config.audio_config();

  if (audio_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return scoped_ptr<AudioEncoder>(new AudioEncoderVerbatim());
  } else if (audio_config.codec == protocol::ChannelConfig::CODEC_SPEEX) {
    return scoped_ptr<AudioEncoder>(new AudioEncoderSpeex());
  } else if (audio_config.codec == protocol::ChannelConfig::CODEC_OPUS) {
    return scoped_ptr<AudioEncoder>(new AudioEncoderOpus());
  }

  NOTIMPLEMENTED();
  return scoped_ptr<AudioEncoder>(NULL);
}

// static
void ClientSessionTraits::Destruct(const ClientSession* client) {
  client->network_task_runner_->DeleteSoon(FROM_HERE, client);
}

}  // namespace remoting
