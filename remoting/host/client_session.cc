// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "remoting/base/capabilities.h"
#include "remoting/base/logging.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/audio_encoder_opus.h"
#include "remoting/codec/audio_encoder_verbatim.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/audio_pump.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/host/mouse_shape_pump.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_thread_proxy.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/video_frame_pump.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

// Default DPI to assume for old clients that use notifyClientDimensions.
const int kDefaultDPI = 96;

namespace remoting {

namespace {

// Name of command-line flag to disable use of I444 by default.
const char kDisableI444SwitchName[] = "disable-i444";

scoped_ptr<AudioEncoder> CreateAudioEncoder(
    const protocol::SessionConfig& config) {
  const protocol::ChannelConfig& audio_config = config.audio_config();

  if (audio_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return make_scoped_ptr(new AudioEncoderVerbatim());
  } else if (audio_config.codec == protocol::ChannelConfig::CODEC_OPUS) {
    return make_scoped_ptr(new AudioEncoderOpus());
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace

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
      connection_(std::move(connection)),
      client_jid_(connection_->session()->jid()),
      desktop_environment_factory_(desktop_environment_factory),
      input_tracker_(&host_input_filter_),
      remote_input_filter_(&input_tracker_),
      mouse_clamping_filter_(&remote_input_filter_),
      disable_input_filter_(&mouse_clamping_filter_),
      disable_clipboard_filter_(clipboard_echo_filter_.host_filter()),
      client_clipboard_factory_(clipboard_echo_filter_.client_filter()),
      max_duration_(max_duration),
      audio_task_runner_(audio_task_runner),
      input_task_runner_(input_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      video_encode_task_runner_(video_encode_task_runner),
      network_task_runner_(network_task_runner),
      ui_task_runner_(ui_task_runner),
      pairing_registry_(pairing_registry),
      is_authenticated_(false),
      pause_video_(false),
      lossless_video_encode_(false),
      // Note that |lossless_video_color_| defaults to true, but actually only
      // controls VP9 video stream color quality.
      lossless_video_color_(!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableI444SwitchName)),
      weak_factory_(this) {
  connection_->SetEventHandler(this);

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
  DCHECK(!audio_pump_);
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(!video_stream_);

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

  // Note that |video_stream_| may be null, depending upon whether
  // extensions choose to wrap or "steal" the video capturer or encoder.
  if (video_control.has_enable()) {
    VLOG(1) << "Received VideoControl (enable="
            << video_control.enable() << ")";
    pause_video_ = !video_control.enable();
    if (video_stream_)
      video_stream_->Pause(pause_video_);
  }
  if (video_control.has_lossless_encode()) {
    VLOG(1) << "Received VideoControl (lossless_encode="
            << video_control.lossless_encode() << ")";
    lossless_video_encode_ = video_control.lossless_encode();
    if (video_stream_)
      video_stream_->SetLosslessEncode(lossless_video_encode_);
  }
  if (video_control.has_lossless_color()) {
    VLOG(1) << "Received VideoControl (lossless_color="
            << video_control.lossless_color() << ")";
    lossless_video_color_ = video_control.lossless_color();
    if (video_stream_)
      video_stream_->SetLosslessColor(lossless_video_color_);
  }
}

void ClientSession::ControlAudio(const protocol::AudioControl& audio_control) {
  DCHECK(CalledOnValidThread());

  if (audio_control.has_enable()) {
    VLOG(1) << "Received AudioControl (enable="
            << audio_control.enable() << ")";
    if (audio_pump_)
      audio_pump_->Pause(!audio_control.enable());
  }
}

void ClientSession::SetCapabilities(
    const protocol::Capabilities& capabilities) {
  DCHECK(CalledOnValidThread());

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
  if (pairing_registry_.get() && pairing_request.has_client_name()) {
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
      if (extension_manager_->OnExtensionMessage(message))
        return;

      DLOG(INFO) << "Unexpected message received: "
                 << message.type() << ": " << message.data();
    }
  }
}

void ClientSession::OnConnectionAuthenticating(
    protocol::ConnectionToClient* connection) {
  event_handler_->OnSessionAuthenticating(this);
}

void ClientSession::OnConnectionAuthenticated(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  DCHECK(!audio_pump_);
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(!video_stream_);

  is_authenticated_ = true;

  if (max_duration_ > base::TimeDelta()) {
    max_duration_timer_.Start(
        FROM_HERE, max_duration_,
        base::Bind(&ClientSession::DisconnectSession, base::Unretained(this),
                   protocol::MAX_SESSION_LENGTH));
  }

  // Notify EventHandler.
  event_handler_->OnSessionAuthenticated(this);

  // Create the desktop environment. Drop the connection if it could not be
  // created for any reason (for instance the curtain could not initialize).
  desktop_environment_ =
      desktop_environment_factory_->Create(weak_factory_.GetWeakPtr());
  if (!desktop_environment_) {
    DisconnectSession(protocol::HOST_CONFIGURATION_ERROR);
    return;
  }

  // Connect host stub.
  connection_->set_host_stub(this);

  // Collate the set of capabilities to offer the client, if it supports them.
  host_capabilities_ = desktop_environment_->GetCapabilities();
  if (!host_capabilities_.empty())
    host_capabilities_.append(" ");
  host_capabilities_.append(extension_manager_->GetCapabilities());

  // Create the object that controls the screen resolution.
  screen_controls_ = desktop_environment_->CreateScreenControls();

  // Create the event executor.
  input_injector_ = desktop_environment_->CreateInputInjector();

  // Connect the host input stubs.
  connection_->set_input_stub(&disable_input_filter_);
  host_input_filter_.set_input_stub(input_injector_.get());

  // Connect the clipboard stubs.
  connection_->set_clipboard_stub(&disable_clipboard_filter_);
  clipboard_echo_filter_.set_host_stub(input_injector_.get());
  clipboard_echo_filter_.set_client_stub(connection_->client_stub());

  // Create a GnubbyAuthHandler to proxy gnubbyd messages.
  gnubby_auth_handler_ = desktop_environment_->CreateGnubbyAuthHandler(
      connection_->client_stub());
}

void ClientSession::OnConnectionChannelsConnected(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  // Negotiate capabilities with the client.
  VLOG(1) << "Host capabilities: " << host_capabilities_;
  protocol::Capabilities capabilities;
  capabilities.set_capabilities(host_capabilities_);
  connection_->client_stub()->SetCapabilities(capabilities);

  // Start the event executor.
  input_injector_->Start(CreateClipboardProxy());
  SetDisableInputs(false);

  // Start recording video.
  ResetVideoPipeline();

  // Create an AudioPump if audio is enabled, to pump audio samples.
  if (connection_->session()->config().is_audio_enabled()) {
    scoped_ptr<AudioEncoder> audio_encoder =
        CreateAudioEncoder(connection_->session()->config());
    audio_pump_.reset(new AudioPump(
        audio_task_runner_, desktop_environment_->CreateAudioCapturer(),
        std::move(audio_encoder), connection_->audio_stub()));
  }

  // Notify the event handler that all our channels are now connected.
  event_handler_->OnSessionChannelsConnected(this);
}

void ClientSession::OnConnectionClosed(
    protocol::ConnectionToClient* connection,
    protocol::ErrorCode error) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  HOST_LOG << "Client disconnected: " << client_jid_ << "; error = " << error;

  // Ignore any further callbacks.
  weak_factory_.InvalidateWeakPtrs();

  // If the client never authenticated then the session failed.
  if (!is_authenticated_)
    event_handler_->OnSessionAuthenticationFailed(this);

  // Ensure that any pressed keys or buttons are released.
  input_tracker_.ReleaseAll();

  // Stop components access the client, audio or video stubs, which are no
  // longer valid once ConnectionToClient calls OnConnectionClosed().
  audio_pump_.reset();
  video_stream_.reset();
  mouse_shape_pump_.reset();
  client_clipboard_factory_.InvalidateWeakPtrs();
  input_injector_.reset();
  screen_controls_.reset();
  desktop_environment_.reset();

  // Notify the ChromotingHost that this client is disconnected.
  event_handler_->OnSessionClosed(this);
}

void ClientSession::OnCreateVideoEncoder(scoped_ptr<VideoEncoder>* encoder) {
  DCHECK(CalledOnValidThread());
  extension_manager_->OnCreateVideoEncoder(encoder);
}

void ClientSession::OnInputEventReceived(
    protocol::ConnectionToClient* connection,
    int64_t event_timestamp) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);

  if (video_stream_.get())
    video_stream_->OnInputEventReceived(event_timestamp);
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

void ClientSession::DisconnectSession(protocol::ErrorCode error) {
  DCHECK(CalledOnValidThread());
  DCHECK(connection_.get());

  max_duration_timer_.Stop();

  // This triggers OnConnectionClosed(), and the session may be destroyed
  // as the result, so this call must be the last in this method.
  connection_->Disconnect(error);
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

  video_stream_.reset();
  mouse_shape_pump_.reset();

  // Create VideoEncoder and DesktopCapturer to match the session's video
  // channel configuration.
  scoped_ptr<webrtc::DesktopCapturer> video_capturer =
      desktop_environment_->CreateVideoCapturer();
  extension_manager_->OnCreateVideoCapturer(&video_capturer);

  // Don't start the video stream if the extension took ownership of the
  // capturer.
  if (!video_capturer)
    return;

  // Create MouseShapePump to send mouse cursor shape.
  // TODO(sergeyu): Move MouseCursorMonitorProxy creation to DesktopEnvironment.
  // When using IpcDesktopCapturer the capture thread is not useful.
  mouse_shape_pump_.reset(
      new MouseShapePump(make_scoped_ptr(new MouseCursorMonitorProxy(
                             video_capture_task_runner_,
                             desktop_environment_->CreateMouseCursorMonitor())),
                         connection_->client_stub()));

  // Create a VideoStream to pump frames from the capturer to the client.
  // TODO(sergeyu): Move DesktopCapturerProxy creation to DesktopEnvironment.
  // When using IpcDesktopCapturer the capture thread is not useful.
  scoped_ptr<webrtc::DesktopCapturer> capturer_proxy(new DesktopCapturerProxy(
      video_capture_task_runner_, std::move(video_capturer)));

  video_stream_ = connection_->StartVideoStream(std::move(capturer_proxy));
  video_stream_->SetSizeCallback(
      base::Bind(&ClientSession::OnScreenSizeChanged, base::Unretained(this)));

  // Apply video-control parameters to the new stream.
  video_stream_->SetLosslessEncode(lossless_video_encode_);
  video_stream_->SetLosslessColor(lossless_video_color_);

  // Pause capturing if necessary.
  video_stream_->Pause(pause_video_);
}

void ClientSession::SetGnubbyAuthHandlerForTesting(
    GnubbyAuthHandler* gnubby_auth_handler) {
  DCHECK(CalledOnValidThread());
  gnubby_auth_handler_.reset(gnubby_auth_handler);
}

scoped_ptr<protocol::ClipboardStub> ClientSession::CreateClipboardProxy() {
  DCHECK(CalledOnValidThread());

  return make_scoped_ptr(
      new protocol::ClipboardThreadProxy(client_clipboard_factory_.GetWeakPtr(),
                                         base::ThreadTaskRunnerHandle::Get()));
}

void ClientSession::OnScreenSizeChanged(const webrtc::DesktopSize& size) {
  DCHECK(CalledOnValidThread());
  mouse_clamping_filter_.set_input_size(size);
  mouse_clamping_filter_.set_output_size(size);
}

}  // namespace remoting
