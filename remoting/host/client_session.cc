// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>

#include "base/message_loop_proxy.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/audio_encoder_opus.h"
#include "remoting/codec/audio_encoder_speex.h"
#include "remoting/codec/audio_encoder_verbatim.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/codec/video_encoder_vp8.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/audio_scheduler.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/host/session_controller.h"
#include "remoting/host/video_scheduler.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_thread_proxy.h"

// Default DPI to assume for old clients that use notifyClientDimensions.
const int kDefaultDPI = 96;

namespace remoting {

ClientSession::ClientSession(
    EventHandler* event_handler,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_ptr<protocol::ConnectionToClient> connection,
    DesktopEnvironmentFactory* desktop_environment_factory,
    const base::TimeDelta& max_duration)
    : event_handler_(event_handler),
      connection_(connection.Pass()),
      connection_factory_(connection_.get()),
      client_jid_(connection_->session()->jid()),
      desktop_environment_factory_(desktop_environment_factory),
      input_tracker_(&host_input_filter_),
      remote_input_filter_(&input_tracker_),
      mouse_clamping_filter_(&remote_input_filter_),
      disable_input_filter_(mouse_clamping_filter_.input_filter()),
      disable_clipboard_filter_(clipboard_echo_filter_.host_filter()),
      auth_input_filter_(&disable_input_filter_),
      auth_clipboard_filter_(&disable_clipboard_filter_),
      client_clipboard_factory_(clipboard_echo_filter_.client_filter()),
      max_duration_(max_duration),
      audio_task_runner_(audio_task_runner),
      input_task_runner_(input_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      video_encode_task_runner_(video_encode_task_runner),
      network_task_runner_(network_task_runner),
      ui_task_runner_(ui_task_runner) {
  connection_->SetEventHandler(this);

  // TODO(sergeyu): Currently ConnectionToClient expects stubs to be
  // set before channels are connected. Make it possible to set stubs
  // later and set them only when connection is authenticated.
  connection_->set_clipboard_stub(&auth_clipboard_filter_);
  connection_->set_host_stub(this);
  connection_->set_input_stub(&auth_input_filter_);

  // |auth_*_filter_|'s states reflect whether the session is authenticated.
  auth_input_filter_.set_enabled(false);
  auth_clipboard_filter_.set_enabled(false);

#if defined(OS_WIN)
  // LocalInputMonitorWin filters out an echo of the injected input before it
  // reaches |remote_input_filter_|.
  remote_input_filter_.SetExpectLocalEcho(false);
#endif  // defined(OS_WIN)
}

ClientSession::~ClientSession() {
  DCHECK(CalledOnValidThread());
  DCHECK(!audio_scheduler_);
  DCHECK(!input_injector_);
  DCHECK(!session_controller_);
  DCHECK(!video_scheduler_);

  connection_.reset();
}

void ClientSession::NotifyClientResolution(
    const protocol::ClientResolution& resolution) {
  if (!resolution.has_dips_width() || !resolution.has_dips_height())
    return;

  VLOG(1) << "Received ClientResolution (dips_width="
          << resolution.dips_width() << ", dips_height="
          << resolution.dips_height() << ")";

  if (!session_controller_)
    return;

  ScreenResolution client_resolution(
      SkISize::Make(resolution.dips_width(), resolution.dips_height()),
      SkIPoint::Make(kDefaultDPI, kDefaultDPI));

  // Try to match the client's resolution.
  if (client_resolution.IsValid())
    session_controller_->SetScreenResolution(client_resolution);
}

void ClientSession::ControlVideo(const protocol::VideoControl& video_control) {
  if (video_control.has_enable()) {
    VLOG(1) << "Received VideoControl (enable="
            << video_control.enable() << ")";
    if (video_scheduler_)
      video_scheduler_->Pause(!video_control.enable());
  }
}

void ClientSession::ControlAudio(const protocol::AudioControl& audio_control) {
  if (audio_control.has_enable()) {
    VLOG(1) << "Received AudioControl (enable="
            << audio_control.enable() << ")";
    if (audio_scheduler_)
      audio_scheduler_->Pause(!audio_control.enable());
  }
}

void ClientSession::OnConnectionAuthenticated(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  auth_input_filter_.set_enabled(true);
  auth_clipboard_filter_.set_enabled(true);

  clipboard_echo_filter_.set_client_stub(connection_->client_stub());
  mouse_clamping_filter_.set_video_stub(connection_->video_stub());

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
  DCHECK(!audio_scheduler_);
  DCHECK(!input_injector_);
  DCHECK(!session_controller_);
  DCHECK(!video_scheduler_);

  scoped_ptr<DesktopEnvironment> desktop_environment =
      desktop_environment_factory_->Create(
          client_jid_,
          base::Bind(&protocol::ConnectionToClient::Disconnect,
                     connection_factory_.GetWeakPtr()));

  // Create the session controller.
  session_controller_ = desktop_environment->CreateSessionController();

  // Create and start the event executor.
  input_injector_ = desktop_environment->CreateInputInjector(
      input_task_runner_, ui_task_runner_);
  input_injector_->Start(CreateClipboardProxy());

  // Connect the host clipboard and input stubs.
  host_input_filter_.set_input_stub(input_injector_.get());
  clipboard_echo_filter_.set_host_stub(input_injector_.get());

  SetDisableInputs(false);

  // Create a VideoEncoder based on the session's video channel configuration.
  scoped_ptr<VideoEncoder> video_encoder =
      CreateVideoEncoder(connection_->session()->config());

  // Create a VideoScheduler to pump frames from the capturer to the client.
  video_scheduler_ = VideoScheduler::Create(
      video_capture_task_runner_,
      video_encode_task_runner_,
      network_task_runner_,
      desktop_environment->CreateVideoCapturer(video_capture_task_runner_,
                                               video_encode_task_runner_),
      video_encoder.Pass(),
      connection_->client_stub(),
      &mouse_clamping_filter_);

  // Create an AudioScheduler if audio is enabled, to pump audio samples.
  if (connection_->session()->config().is_audio_enabled()) {
    scoped_ptr<AudioEncoder> audio_encoder =
        CreateAudioEncoder(connection_->session()->config());
    audio_scheduler_ = AudioScheduler::Create(
        audio_task_runner_,
        network_task_runner_,
        desktop_environment->CreateAudioCapturer(audio_task_runner_),
        audio_encoder.Pass(),
        connection_->audio_stub());
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
  if (audio_scheduler_) {
    audio_scheduler_->Stop();
    audio_scheduler_ = NULL;
  }
  if (video_scheduler_) {
    video_scheduler_->Stop();
    video_scheduler_ = NULL;
  }

  client_clipboard_factory_.InvalidateWeakPtrs();
  input_injector_.reset();
  session_controller_.reset();

  // Notify the ChromotingHost that this client is disconnected.
  // TODO(sergeyu): Log failure reason?
  event_handler_->OnSessionClosed(this);
}

void ClientSession::OnSequenceNumberUpdated(
    protocol::ConnectionToClient* connection, int64 sequence_number) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  if (video_scheduler_)
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

scoped_ptr<protocol::ClipboardStub> ClientSession::CreateClipboardProxy() {
  DCHECK(CalledOnValidThread());

  return scoped_ptr<protocol::ClipboardStub>(
      new protocol::ClipboardThreadProxy(
          client_clipboard_factory_.GetWeakPtr(),
          base::MessageLoopProxy::current()));
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

}  // namespace remoting
