// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>

#include "base/message_loop/message_loop_proxy.h"
#include "remoting/base/capabilities.h"
#include "remoting/base/logging.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/audio_encoder_opus.h"
#include "remoting/codec/audio_encoder_verbatim.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/codec/video_encoder_vpx.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/audio_scheduler.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/host/video_scheduler.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_thread_proxy.h"
#include "remoting/protocol/pairing_registry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

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
    const base::TimeDelta& max_duration,
    scoped_refptr<protocol::PairingRegistry> pairing_registry,
    const std::vector<HostExtension*>& extensions)
    : event_handler_(event_handler),
      connection_(connection.Pass()),
      client_jid_(connection_->session()->jid()),
      control_factory_(this),
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
      ui_task_runner_(ui_task_runner),
      pairing_registry_(pairing_registry),
      pause_video_(false),
      lossless_video_encode_(false),
      lossless_video_color_(false) {
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

  // Create a manager for the configured extensions, if any.
  extension_manager_.reset(new HostExtensionSessionManager(extensions, this));

#if defined(OS_WIN)
  // LocalInputMonitorWin filters out an echo of the injected input before it
  // reaches |remote_input_filter_|.
  remote_input_filter_.SetExpectLocalEcho(false);
#endif  // defined(OS_WIN)
}

ClientSession::~ClientSession() {
  DCHECK(CalledOnValidThread());
  DCHECK(!audio_scheduler_.get());
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(!video_scheduler_.get());

  connection_.reset();
}

void ClientSession::NotifyClientResolution(
    const protocol::ClientResolution& resolution) {
  DCHECK(CalledOnValidThread());

  // TODO(sergeyu): Move these checks to protocol layer.
  if (!resolution.has_dips_width() || !resolution.has_dips_height() ||
      resolution.dips_width() < 0 || resolution.dips_height() < 0 ||
      resolution.width() <= 0 || resolution.height() <= 0) {
    LOG(ERROR) << "Received invalid ClientResolution message.";
    return;
  }

  VLOG(1) << "Received ClientResolution (dips_width="
          << resolution.dips_width() << ", dips_height="
          << resolution.dips_height() << ")";

  if (!screen_controls_)
    return;

  ScreenResolution client_resolution(
      webrtc::DesktopSize(resolution.dips_width(), resolution.dips_height()),
      webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));

  // Try to match the client's resolution.
  screen_controls_->SetScreenResolution(client_resolution);
}

void ClientSession::ControlVideo(const protocol::VideoControl& video_control) {
  DCHECK(CalledOnValidThread());

  // Note that |video_scheduler_| may be NULL, depending upon whether extensions
  // choose to wrap or "steal" the video capturer or encoder.
  if (video_control.has_enable()) {
    VLOG(1) << "Received VideoControl (enable="
            << video_control.enable() << ")";
    pause_video_ = !video_control.enable();
    if (video_scheduler_)
      video_scheduler_->Pause(pause_video_);
  }
  if (video_control.has_lossless_encode()) {
    VLOG(1) << "Received VideoControl (lossless_encode="
            << video_control.lossless_encode() << ")";
    lossless_video_encode_ = video_control.lossless_encode();
    if (video_scheduler_)
      video_scheduler_->SetLosslessEncode(lossless_video_encode_);
  }
  if (video_control.has_lossless_color()) {
    VLOG(1) << "Received VideoControl (lossless_color="
            << video_control.lossless_color() << ")";
    lossless_video_color_ = video_control.lossless_color();
    if (video_scheduler_)
      video_scheduler_->SetLosslessColor(lossless_video_color_);
  }
}

void ClientSession::ControlAudio(const protocol::AudioControl& audio_control) {
  DCHECK(CalledOnValidThread());

  if (audio_control.has_enable()) {
    VLOG(1) << "Received AudioControl (enable="
            << audio_control.enable() << ")";
    if (audio_scheduler_.get())
      audio_scheduler_->Pause(!audio_control.enable());
  }
}

void ClientSession::SetCapabilities(
    const protocol::Capabilities& capabilities) {
  DCHECK(CalledOnValidThread());

  // The client should not send protocol::Capabilities if it is not supported by
  // the config channel.
  if (!connection_->session()->config().SupportsCapabilities()) {
    LOG(ERROR) << "Unexpected protocol::Capabilities has been received.";
    return;
  }

  // Ignore all the messages but the 1st one.
  if (client_capabilities_) {
    LOG(WARNING) << "protocol::Capabilities has been received already.";
    return;
  }

  // Compute the set of capabilities supported by both client and host.
  client_capabilities_ = make_scoped_ptr(new std::string());
  if (capabilities.has_capabilities())
    *client_capabilities_ = capabilities.capabilities();
  capabilities_ = IntersectCapabilities(*client_capabilities_,
                                        host_capabilities_);
  extension_manager_->OnNegotiatedCapabilities(
      connection_->client_stub(), capabilities_);

  VLOG(1) << "Client capabilities: " << *client_capabilities_;

  // Calculate the set of capabilities enabled by both client and host and
  // pass it to the desktop environment if it is available.
  desktop_environment_->SetCapabilities(capabilities_);
}

void ClientSession::RequestPairing(
    const protocol::PairingRequest& pairing_request) {
  if (pairing_registry_ && pairing_request.has_client_name()) {
    protocol::PairingRegistry::Pairing pairing =
        pairing_registry_->CreatePairing(pairing_request.client_name());
    protocol::PairingResponse pairing_response;
    pairing_response.set_client_id(pairing.client_id());
    pairing_response.set_shared_secret(pairing.shared_secret());
    connection_->client_stub()->SetPairingResponse(pairing_response);
  }
}

void ClientSession::DeliverClientMessage(
    const protocol::ExtensionMessage& message) {
  if (message.has_type()) {
    if (message.type() == "test-echo") {
      protocol::ExtensionMessage reply;
      reply.set_type("test-echo-reply");
      if (message.has_data())
        reply.set_data(message.data().substr(0, 16));
      connection_->client_stub()->DeliverHostMessage(reply);
      return;
    } else if (message.type() == "gnubby-auth") {
      if (gnubby_auth_handler_) {
        gnubby_auth_handler_->DeliverClientMessage(message.data());
      } else {
        HOST_LOG << "gnubby auth is not enabled";
      }
      return;
    } else {
      extension_manager_->OnExtensionMessage(message);
      return;
    }
  }
  HOST_LOG << "Unexpected message received: "
            << message.type() << ": " << message.data();
}

void ClientSession::OnConnectionAuthenticating(
    protocol::ConnectionToClient* connection) {
  event_handler_->OnSessionAuthenticating(this);
}

void ClientSession::OnConnectionAuthenticated(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  DCHECK(!audio_scheduler_.get());
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(!video_scheduler_.get());

  auth_input_filter_.set_enabled(true);
  auth_clipboard_filter_.set_enabled(true);

  clipboard_echo_filter_.set_client_stub(connection_->client_stub());
  mouse_clamping_filter_.set_video_stub(connection_->video_stub());

  if (max_duration_ > base::TimeDelta()) {
    // TODO(simonmorris): Let Disconnect() tell the client that the
    // disconnection was caused by the session exceeding its maximum duration.
    max_duration_timer_.Start(FROM_HERE, max_duration_,
                              this, &ClientSession::DisconnectSession);
  }

  // Disconnect the session if the connection was rejected by the host.
  if (!event_handler_->OnSessionAuthenticated(this)) {
    DisconnectSession();
    return;
  }

  // Create the desktop environment. Drop the connection if it could not be
  // created for any reason (for instance the curtain could not initialize).
  desktop_environment_ =
      desktop_environment_factory_->Create(control_factory_.GetWeakPtr());
  if (!desktop_environment_) {
    DisconnectSession();
    return;
  }

  // Collate the set of capabilities to offer the client, if it supports them.
  if (connection_->session()->config().SupportsCapabilities()) {
    host_capabilities_ = desktop_environment_->GetCapabilities();
    if (!host_capabilities_.empty()) {
      host_capabilities_.append(" ");
    }
    host_capabilities_.append(extension_manager_->GetCapabilities());
  } else {
    VLOG(1) << "The client does not support any capabilities.";
    desktop_environment_->SetCapabilities(std::string());
  }

  // Create the object that controls the screen resolution.
  screen_controls_ = desktop_environment_->CreateScreenControls();

  // Create the event executor.
  input_injector_ = desktop_environment_->CreateInputInjector();

  // Connect the host clipboard and input stubs.
  host_input_filter_.set_input_stub(input_injector_.get());
  clipboard_echo_filter_.set_host_stub(input_injector_.get());

  // Create an AudioScheduler if audio is enabled, to pump audio samples.
  if (connection_->session()->config().is_audio_enabled()) {
    scoped_ptr<AudioEncoder> audio_encoder =
        CreateAudioEncoder(connection_->session()->config());
    audio_scheduler_ = new AudioScheduler(
        audio_task_runner_,
        network_task_runner_,
        desktop_environment_->CreateAudioCapturer(),
        audio_encoder.Pass(),
        connection_->audio_stub());
  }

  // Create a GnubbyAuthHandler to proxy gnubbyd messages.
  gnubby_auth_handler_ = desktop_environment_->CreateGnubbyAuthHandler(
      connection_->client_stub());
}

void ClientSession::OnConnectionChannelsConnected(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  // Negotiate capabilities with the client.
  if (connection_->session()->config().SupportsCapabilities()) {
    VLOG(1) << "Host capabilities: " << host_capabilities_;

    protocol::Capabilities capabilities;
    capabilities.set_capabilities(host_capabilities_);
    connection_->client_stub()->SetCapabilities(capabilities);
  }

  // Start the event executor.
  input_injector_->Start(CreateClipboardProxy());
  SetDisableInputs(false);

  // Start recording video.
  ResetVideoPipeline();

  // Start recording audio.
  if (connection_->session()->config().is_audio_enabled())
    audio_scheduler_->Start();

  // Notify the event handler that all our channels are now connected.
  event_handler_->OnSessionChannelsConnected(this);
}

void ClientSession::OnConnectionClosed(
    protocol::ConnectionToClient* connection,
    protocol::ErrorCode error) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  // Ignore any further callbacks.
  control_factory_.InvalidateWeakPtrs();

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
    audio_scheduler_->Stop();
    audio_scheduler_ = NULL;
  }
  if (video_scheduler_.get()) {
    video_scheduler_->Stop();
    video_scheduler_ = NULL;
  }

  client_clipboard_factory_.InvalidateWeakPtrs();
  input_injector_.reset();
  screen_controls_.reset();
  desktop_environment_.reset();

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
}

void ClientSession::OnRouteChange(
    protocol::ConnectionToClient* connection,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  event_handler_->OnSessionRouteChange(this, channel_name, route);
}

const std::string& ClientSession::client_jid() const {
  return client_jid_;
}

void ClientSession::DisconnectSession() {
  DCHECK(CalledOnValidThread());
  DCHECK(connection_.get());

  max_duration_timer_.Stop();

  // This triggers OnConnectionClosed(), and the session may be destroyed
  // as the result, so this call must be the last in this method.
  connection_->Disconnect();
}

void ClientSession::OnLocalMouseMoved(const webrtc::DesktopVector& position) {
  DCHECK(CalledOnValidThread());
  remote_input_filter_.LocalMouseMoved(position);
}

void ClientSession::SetDisableInputs(bool disable_inputs) {
  DCHECK(CalledOnValidThread());

  if (disable_inputs)
    input_tracker_.ReleaseAll();

  disable_input_filter_.set_enabled(!disable_inputs);
  disable_clipboard_filter_.set_enabled(!disable_inputs);
}

void ClientSession::ResetVideoPipeline() {
  DCHECK(CalledOnValidThread());

  if (video_scheduler_.get()) {
    video_scheduler_->Stop();
    video_scheduler_ = NULL;
  }

  // Create VideoEncoder and ScreenCapturer to match the session's video channel
  // configuration.
  scoped_ptr<webrtc::ScreenCapturer> video_capturer =
      extension_manager_->OnCreateVideoCapturer(
          desktop_environment_->CreateVideoCapturer());
  scoped_ptr<VideoEncoder> video_encoder =
      extension_manager_->OnCreateVideoEncoder(
          CreateVideoEncoder(connection_->session()->config()));

  // Don't start the VideoScheduler if either capturer or encoder are missing.
  if (!video_capturer || !video_encoder)
    return;

  // Create a VideoScheduler to pump frames from the capturer to the client.
  video_scheduler_ = new VideoScheduler(
      video_capture_task_runner_,
      video_encode_task_runner_,
      network_task_runner_,
      video_capturer.Pass(),
      desktop_environment_->CreateMouseCursorMonitor(),
      video_encoder.Pass(),
      connection_->client_stub(),
      &mouse_clamping_filter_);

  // Apply video-control parameters to the new scheduler.
  video_scheduler_->Pause(pause_video_);
  video_scheduler_->SetLosslessEncode(lossless_video_encode_);
  video_scheduler_->SetLosslessColor(lossless_video_color_);

  // Start capturing the screen.
  video_scheduler_->Start();
}

void ClientSession::SetGnubbyAuthHandlerForTesting(
    GnubbyAuthHandler* gnubby_auth_handler) {
  DCHECK(CalledOnValidThread());
  gnubby_auth_handler_.reset(gnubby_auth_handler);
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

  if (video_config.codec == protocol::ChannelConfig::CODEC_VP8) {
    return remoting::VideoEncoderVpx::CreateForVP8().PassAs<VideoEncoder>();
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_VP9) {
    return remoting::VideoEncoderVpx::CreateForVP9().PassAs<VideoEncoder>();
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return scoped_ptr<VideoEncoder>(new remoting::VideoEncoderVerbatim());
  }

  NOTREACHED();
  return scoped_ptr<VideoEncoder>();
}

// static
scoped_ptr<AudioEncoder> ClientSession::CreateAudioEncoder(
    const protocol::SessionConfig& config) {
  const protocol::ChannelConfig& audio_config = config.audio_config();

  if (audio_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return scoped_ptr<AudioEncoder>(new AudioEncoderVerbatim());
  } else if (audio_config.codec == protocol::ChannelConfig::CODEC_OPUS) {
    return scoped_ptr<AudioEncoder>(new AudioEncoderOpus());
  }

  NOTREACHED();
  return scoped_ptr<AudioEncoder>();
}

}  // namespace remoting
