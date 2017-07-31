// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_session.h"

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/client_telemetry_logger.h"
#include "remoting/client/input/native_device_keymap.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/signaling/server_log_entry.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

const char* const kXmppServer = "talk.google.com";
const int kXmppPort = 5222;
const bool kXmppUseTls = true;

// Interval at which to log performance statistics, if enabled.
const int kPerfStatsIntervalMs = 60000;

// Default DPI to assume for old clients that use notifyClientResolution.
const int kDefaultDPI = 96;

// Used by NormalizeclientResolution. See comment below.
const int kMinDimension = 640;

// Normalizes the resolution so that both dimensions are not smaller than
// kMinDimension.
void NormalizeClientResolution(protocol::ClientResolution* resolution) {
  int min_dimension =
      std::min(resolution->dips_width(), resolution->dips_height());
  if (min_dimension >= kMinDimension) {
    return;
  }

  // Always scale by integer to prevent blurry interpolation.
  int scale = std::ceil(((float)kMinDimension) / min_dimension);
  resolution->set_dips_width(resolution->dips_width() * scale);
  resolution->set_dips_height(resolution->dips_height() * scale);
}

}  // namespace

ChromotingSession::ChromotingSession(
    base::WeakPtr<ChromotingSession::Delegate> delegate,
    std::unique_ptr<protocol::CursorShapeStub> cursor_shape_stub,
    std::unique_ptr<protocol::VideoRenderer> video_renderer,
    base::WeakPtr<protocol::AudioStub> audio_player,
    const ConnectToHostInfo& info,
    const protocol::ClientAuthenticationConfig& client_auth_config)
    : delegate_(delegate),
      connection_info_(info),
      client_auth_config_(client_auth_config),
      cursor_shape_stub_(std::move(cursor_shape_stub)),
      video_renderer_(std::move(video_renderer)),
      audio_player_(audio_player),
      capabilities_(info.capabilities),
      weak_factory_(this) {
  runtime_ = ChromotingClientRuntime::GetInstance();
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  weak_ptr_ = weak_factory_.GetWeakPtr();

  // Initialize XMPP config.
  xmpp_config_.host = kXmppServer;
  xmpp_config_.port = kXmppPort;
  xmpp_config_.use_tls = kXmppUseTls;
  xmpp_config_.username = info.username;
  xmpp_config_.auth_token = info.auth_token;

  client_auth_config_.fetch_third_party_token_callback = base::Bind(
      &ChromotingSession::FetchThirdPartyToken, GetWeakPtr(), info.host_pubkey);
}

ChromotingSession::~ChromotingSession() {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());
  if (client_) {
    ReleaseResources();
  }
}

void ChromotingSession::Connect() {
  if (runtime_->network_task_runner()->BelongsToCurrentThread()) {
    ConnectToHostOnNetworkThread();
  } else {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::ConnectToHostOnNetworkThread,
                              GetWeakPtr()));
  }
}

void ChromotingSession::Disconnect() {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::Disconnect, GetWeakPtr()));
    return;
  }

  stats_logging_enabled_ = false;

  // User disconnection will not trigger OnConnectionState(Closed, OK).
  // Remote disconnection will trigger OnConnectionState(...) and later trigger
  // Disconnect().
  if (connected_) {
    logger_->LogSessionStateChange(ChromotingEvent::SessionState::CLOSED,
                                   ChromotingEvent::ConnectionError::NONE);
    connected_ = false;
  }

  ReleaseResources();
}

void ChromotingSession::FetchThirdPartyToken(
    const std::string& host_public_key,
    const std::string& token_url,
    const std::string& scope,
    const protocol::ThirdPartyTokenFetchedCallback& token_fetched_callback) {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(third_party_token_fetched_callback_.is_null());

  third_party_token_fetched_callback_ = token_fetched_callback;
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ChromotingSession::Delegate::FetchThirdPartyToken,
                            delegate_, token_url, host_public_key, scope));
}

void ChromotingSession::HandleOnThirdPartyTokenFetched(
    const std::string& token,
    const std::string& shared_secret) {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());

  if (!third_party_token_fetched_callback_.is_null()) {
    base::ResetAndReturn(&third_party_token_fetched_callback_)
        .Run(token, shared_secret);
  } else {
    LOG(WARNING) << "ThirdPartyAuth: Ignored OnThirdPartyTokenFetched()"
                    " without a pending fetch.";
  }
}

void ChromotingSession::ProvideSecret(const std::string& pin,
                                      bool create_pairing,
                                      const std::string& device_name) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());

  // TODO(nicholss): |pin| here is not used. Maybe there was an api refactor and
  // this was not cleaned up. The auth pin providing mechanism seems to be call
  // ProvideSecret, and then call the auth callback. When session moves to
  // Connected state, this chromoing session calls RequestPairing  based on
  // create_pairing.

  create_pairing_ = create_pairing;

  if (create_pairing)
    SetDeviceName(device_name);
}

void ChromotingSession::SendMouseEvent(int x,
                                       int y,
                                       protocol::MouseEvent_MouseButton button,
                                       bool button_down) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::SendMouseEvent, GetWeakPtr(),
                              x, y, button, button_down));
    return;
  }

  protocol::MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  event.set_button(button);
  if (button != protocol::MouseEvent::BUTTON_UNDEFINED)
    event.set_button_down(button_down);

  client_->input_stub()->InjectMouseEvent(event);
}

void ChromotingSession::SendMouseWheelEvent(int delta_x, int delta_y) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::SendMouseWheelEvent,
                              GetWeakPtr(), delta_x, delta_y));
    return;
  }

  protocol::MouseEvent event;
  event.set_wheel_delta_x(delta_x);
  event.set_wheel_delta_y(delta_y);
  client_->input_stub()->InjectMouseEvent(event);
}

bool ChromotingSession::SendKeyEvent(int scan_code,
                                     int key_code,
                                     bool key_down) {
  // For software keyboards |scan_code| is set to 0, in which case the
  // |key_code| is used instead.
  uint32_t usb_key_code =
      scan_code ? ui::KeycodeConverter::NativeKeycodeToUsbKeycode(scan_code)
                : NativeDeviceKeycodeToUsbKeycode(key_code);
  if (!usb_key_code) {
    LOG(WARNING) << "Ignoring unknown key code: " << key_code
                 << " scan code: " << scan_code;
    return false;
  }

  SendKeyEventInternal(usb_key_code, key_down);
  return true;
}

void ChromotingSession::SendTextEvent(const std::string& text) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingSession::SendTextEvent, GetWeakPtr(), text));
    return;
  }

  protocol::TextEvent event;
  event.set_text(text);
  client_->input_stub()->InjectTextEvent(event);
}

void ChromotingSession::SendTouchEvent(
    const protocol::TouchEvent& touch_event) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::SendTouchEvent, GetWeakPtr(),
                              touch_event));
    return;
  }

  client_->input_stub()->InjectTouchEvent(touch_event);
}

void ChromotingSession::SendClientResolution(int dips_width,
                                             int dips_height,
                                             int scale) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::SendClientResolution,
                              GetWeakPtr(), dips_width, dips_height, scale));
    return;
  }

  protocol::ClientResolution client_resolution;
  client_resolution.set_dips_width(dips_width);
  client_resolution.set_dips_height(dips_height);
  client_resolution.set_x_dpi(scale * kDefaultDPI);
  client_resolution.set_y_dpi(scale * kDefaultDPI);
  NormalizeClientResolution(&client_resolution);

  // Include the legacy width & height in physical pixels for use by older
  // hosts.
  client_resolution.set_width_deprecated(dips_width * scale);
  client_resolution.set_height_deprecated(dips_height * scale);

  client_->host_stub()->NotifyClientResolution(client_resolution);
}

void ChromotingSession::EnableVideoChannel(bool enable) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::EnableVideoChannel,
                              GetWeakPtr(), enable));
    return;
  }

  protocol::VideoControl video_control;
  video_control.set_enable(enable);
  client_->host_stub()->ControlVideo(video_control);
}

void ChromotingSession::SendClientMessage(const std::string& type,
                                          const std::string& data) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::SendClientMessage,
                              GetWeakPtr(), type, data));
    return;
  }

  protocol::ExtensionMessage extension_message;
  extension_message.set_type(type);
  extension_message.set_data(data);
  client_->host_stub()->DeliverClientMessage(extension_message);
}

void ChromotingSession::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());

  // This code assumes no intermediate connection state between CONNECTED and
  // CLOSED/FAILED.
  connected_ = state == protocol::ConnectionToHost::CONNECTED;
  EnableStatsLogging(connected_);

  logger_->LogSessionStateChange(ClientTelemetryLogger::TranslateState(state),
                                 ClientTelemetryLogger::TranslateError(error));

  if (create_pairing_ && state == protocol::ConnectionToHost::CONNECTED) {
    protocol::PairingRequest request;
    DCHECK(!device_name_.empty());
    request.set_client_name(device_name_);
    client_->host_stub()->RequestPairing(request);
  }

  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ChromotingSession::Delegate::OnConnectionState,
                            delegate_, state, error));
}

void ChromotingSession::OnConnectionReady(bool ready) {
  // We ignore this message, since OnConnectionState tells us the same thing.
}

void ChromotingSession::OnRouteChanged(const std::string& channel_name,
                                       const protocol::TransportRoute& route) {
  std::string message = "Channel " + channel_name + " using " +
                        protocol::TransportRoute::GetTypeString(route.type) +
                        " connection.";
  VLOG(1) << "Route: " << message;
}

void ChromotingSession::SetCapabilities(const std::string& capabilities) {
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ChromotingSession::Delegate::SetCapabilities,
                            delegate_, capabilities));
}

void ChromotingSession::SetPairingResponse(
    const protocol::PairingResponse& response) {
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingSession::Delegate::CommitPairingCredentials,
                 delegate_, client_auth_config_.host_id, response.client_id(),
                 response.shared_secret()));
}

void ChromotingSession::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingSession::Delegate::HandleExtensionMessage,
                 delegate_, message.type(), message.data()));
}

void ChromotingSession::SetDesktopSize(const webrtc::DesktopSize& size,
                                       const webrtc::DesktopVector& dpi) {
  // ChromotingSession's VideoRenderer gets size from the frames and it doesn't
  // use DPI, so this call can be ignored.
}

protocol::ClipboardStub* ChromotingSession::GetClipboardStub() {
  return this;
}

protocol::CursorShapeStub* ChromotingSession::GetCursorShapeStub() {
  return cursor_shape_stub_.get();
}

void ChromotingSession::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

base::WeakPtr<ChromotingSession> ChromotingSession::GetWeakPtr() {
  return weak_ptr_;
}

void ChromotingSession::ConnectToHostOnNetworkThread() {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  client_context_.reset(new ClientContext(runtime_->network_task_runner()));
  client_context_->Start();

  perf_tracker_.reset(new protocol::PerformanceTracker());

  video_renderer_->Initialize(*client_context_, perf_tracker_.get());

  logger_.reset(new ClientTelemetryLogger(runtime_->log_writer(),
                                          ChromotingEvent::Mode::ME2ME));
  logger_->SetHostInfo(
      connection_info_.host_version,
      ChromotingEvent::ParseOsFromString(connection_info_.host_os),
      connection_info_.host_os_version);

  client_.reset(new ChromotingClient(client_context_.get(), this,
                                     video_renderer_.get(), audio_player_));

  signaling_.reset(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             runtime_->url_requester(), xmpp_config_));

  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          signaling_.get(),
          base::MakeUnique<protocol::ChromiumPortAllocatorFactory>(),
          base::MakeUnique<ChromiumUrlRequestFactory>(
              runtime_->url_requester()),
          protocol::NetworkSettings(
              protocol::NetworkSettings::NAT_TRAVERSAL_FULL),
          protocol::TransportRole::CLIENT);

#if defined(ENABLE_WEBRTC_REMOTING_CLIENT)
  if (connection_info_.flags.find("useWebrtc") != std::string::npos) {
    VLOG(0) << "Attempting to connect using WebRTC.";
    std::unique_ptr<protocol::CandidateSessionConfig> protocol_config =
        protocol::CandidateSessionConfig::CreateEmpty();
    protocol_config->set_webrtc_supported(true);
    protocol_config->set_ice_supported(false);
    client_->set_protocol_config(std::move(protocol_config));
  }
#endif  // defined(ENABLE_WEBRTC_REMOTING_CLIENT)
  client_->Start(signaling_.get(), client_auth_config_, transport_context,
                 connection_info_.host_jid, capabilities_);
}

void ChromotingSession::SetDeviceName(const std::string& device_name) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::SetDeviceName, GetWeakPtr(),
                              device_name));
    return;
  }

  device_name_ = device_name;
}

void ChromotingSession::SendKeyEventInternal(int usb_key_code, bool key_down) {
  if (!runtime_->network_task_runner()->BelongsToCurrentThread()) {
    runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingSession::SendKeyEventInternal,
                              GetWeakPtr(), usb_key_code, key_down));
    return;
  }

  protocol::KeyEvent event;
  event.set_usb_keycode(usb_key_code);
  event.set_pressed(key_down);
  client_->input_stub()->InjectKeyEvent(event);
}

void ChromotingSession::EnableStatsLogging(bool enabled) {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());

  if (enabled && !stats_logging_enabled_) {
    runtime_->network_task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&ChromotingSession::LogPerfStats, GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));
  }
  stats_logging_enabled_ = enabled;
}

void ChromotingSession::LogPerfStats() {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());

  if (!stats_logging_enabled_)
    return;

  logger_->LogStatistics(perf_tracker_.get());

  runtime_->network_task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromotingSession::LogPerfStats, GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));
}

void ChromotingSession::ReleaseResources() {
  logger_.reset();

  // |client_| must be torn down before |signaling_|.
  client_.reset();
  delegate_.reset();
  audio_player_.reset();
  video_renderer_.reset();
  signaling_.reset();
  perf_tracker_.reset();
  client_context_.reset();
  cursor_shape_stub_.reset();

  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace remoting
