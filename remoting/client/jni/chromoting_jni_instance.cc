// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_instance.h"

#include <android/log.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/client/audio_player_android.h"
#include "remoting/client/client_telemetry_logger.h"
#include "remoting/client/jni/android_keymap.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_client.h"
#include "remoting/client/jni/jni_pairing_secret_fetcher.h"
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

// TODO(solb) Move into location shared with client plugin.
const char* const kXmppServer = "talk.google.com";
const int kXmppPort = 5222;
const bool kXmppUseTls = true;

// Interval at which to log performance statistics, if enabled.
const int kPerfStatsIntervalMs = 60000;

}  // namespace

ChromotingJniInstance::ChromotingJniInstance(
    ChromotingJniRuntime* jni_runtime,
    base::WeakPtr<JniClient> jni_client,
    base::WeakPtr<JniPairingSecretFetcher> secret_fetcher,
    std::unique_ptr<protocol::CursorShapeStub> cursor_shape_stub,
    std::unique_ptr<protocol::VideoRenderer> video_renderer,
    const ConnectToHostInfo& info)
    : jni_runtime_(jni_runtime),
      jni_client_(jni_client),
      secret_fetcher_(secret_fetcher),
      connection_info_(info),
      cursor_shape_stub_(std::move(cursor_shape_stub)),
      video_renderer_(std::move(video_renderer)),
      capabilities_(info.capabilities),
      weak_factory_(this) {
  DCHECK(jni_runtime_->ui_task_runner()->BelongsToCurrentThread());
  weak_ptr_ = weak_factory_.GetWeakPtr();

  // Initialize XMPP config.
  xmpp_config_.host = kXmppServer;
  xmpp_config_.port = kXmppPort;
  xmpp_config_.use_tls = kXmppUseTls;
  xmpp_config_.username = info.username;
  xmpp_config_.auth_token = info.auth_token;

  client_auth_config_.host_id = info.host_id;
  client_auth_config_.pairing_client_id = info.pairing_id;
  client_auth_config_.pairing_secret = info.pairing_secret;
  client_auth_config_.fetch_secret_callback =
      base::Bind(&JniPairingSecretFetcher::FetchSecret, secret_fetcher);
  client_auth_config_.fetch_third_party_token_callback = base::Bind(
      &ChromotingJniInstance::FetchThirdPartyToken, GetWeakPtr(),
      info.host_pubkey);
}

ChromotingJniInstance::~ChromotingJniInstance() {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());
  if (client_) {
    ReleaseResources();
  }
}

void ChromotingJniInstance::Connect() {
  if (jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    ConnectToHostOnNetworkThread();
  } else {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::ConnectToHostOnNetworkThread,
                   GetWeakPtr()));
  }
}

void ChromotingJniInstance::Disconnect() {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::Disconnect, GetWeakPtr()));
    return;
  }

  stats_logging_enabled_ = false;

  // User disconnection will not trigger OnConnectionState(Closed, OK).
  // Remote disconnection will trigger OnConnectionState(...) and later trigger
  // Disconnect().
  if (connected_) {
    logger_->LogSessionStateChange(
        ChromotingEvent::SessionState::CLOSED,
        ChromotingEvent::ConnectionError::NONE);
    connected_ = false;
  }

  ReleaseResources();
}

void ChromotingJniInstance::FetchThirdPartyToken(
    const std::string& host_public_key,
    const std::string& token_url,
    const std::string& scope,
    const protocol::ThirdPartyTokenFetchedCallback& token_fetched_callback) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(third_party_token_fetched_callback_.is_null());

  __android_log_print(ANDROID_LOG_INFO,
                      "ThirdPartyAuth",
                      "Fetching Third Party Token from user.");

  third_party_token_fetched_callback_ = token_fetched_callback;
  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&JniClient::FetchThirdPartyToken, jni_client_,
                            token_url, host_public_key, scope));
}

void ChromotingJniInstance::HandleOnThirdPartyTokenFetched(
    const std::string& token,
    const std::string& shared_secret) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  __android_log_print(
      ANDROID_LOG_INFO, "ThirdPartyAuth", "Third Party Token Fetched.");

  if (!third_party_token_fetched_callback_.is_null()) {
    base::ResetAndReturn(&third_party_token_fetched_callback_)
        .Run(token, shared_secret);
  } else {
    __android_log_print(
        ANDROID_LOG_WARN,
        "ThirdPartyAuth",
        "Ignored OnThirdPartyTokenFetched() without a pending fetch.");
  }
}

void ChromotingJniInstance::ProvideSecret(const std::string& pin,
                                          bool create_pairing,
                                          const std::string& device_name) {
  DCHECK(jni_runtime_->ui_task_runner()->BelongsToCurrentThread());

  create_pairing_ = create_pairing;

  if (create_pairing)
    SetDeviceName(device_name);

  jni_runtime_->network_task_runner()->PostTask(
      FROM_HERE, base::Bind(&JniPairingSecretFetcher::ProvideSecret,
                            secret_fetcher_, pin));
}

void ChromotingJniInstance::SendMouseEvent(
    int x, int y,
    protocol::MouseEvent_MouseButton button,
    bool button_down) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SendMouseEvent,
                              GetWeakPtr(), x, y, button, button_down));
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

void ChromotingJniInstance::SendMouseWheelEvent(int delta_x, int delta_y) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SendMouseWheelEvent,
                              GetWeakPtr(), delta_x, delta_y));
    return;
  }

  protocol::MouseEvent event;
  event.set_wheel_delta_x(delta_x);
  event.set_wheel_delta_y(delta_y);
  client_->input_stub()->InjectMouseEvent(event);
}

bool ChromotingJniInstance::SendKeyEvent(int scan_code,
                                         int key_code,
                                         bool key_down) {
  // For software keyboards |scan_code| is set to 0, in which case the
  // |key_code| is used instead.
  uint32_t usb_key_code =
      scan_code ? ui::KeycodeConverter::NativeKeycodeToUsbKeycode(scan_code)
                : AndroidKeycodeToUsbKeycode(key_code);
  if (!usb_key_code) {
    LOG(WARNING) << "Ignoring unknown key code: " << key_code
                 << " scan code: " << scan_code;
    return false;
  }

  SendKeyEventInternal(usb_key_code, key_down);
  return true;
}

void ChromotingJniInstance::SendTextEvent(const std::string& text) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::SendTextEvent, GetWeakPtr(), text));
    return;
  }

  protocol::TextEvent event;
  event.set_text(text);
  client_->input_stub()->InjectTextEvent(event);
}

void ChromotingJniInstance::SendTouchEvent(
    const protocol::TouchEvent& touch_event) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SendTouchEvent,
                              GetWeakPtr(), touch_event));
    return;
  }

  client_->input_stub()->InjectTouchEvent(touch_event);
}

void ChromotingJniInstance::EnableVideoChannel(bool enable) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::EnableVideoChannel,
                              GetWeakPtr(), enable));
    return;
  }

  protocol::VideoControl video_control;
  video_control.set_enable(enable);
  client_->host_stub()->ControlVideo(video_control);
}

void ChromotingJniInstance::SendClientMessage(const std::string& type,
                                              const std::string& data) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SendClientMessage,
                              GetWeakPtr(), type, data));
    return;
  }

  protocol::ExtensionMessage extension_message;
  extension_message.set_type(type);
  extension_message.set_data(data);
  client_->host_stub()->DeliverClientMessage(extension_message);
}

void ChromotingJniInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  // This code assumes no intermediate connection state between CONNECTED and
  // CLOSED/FAILED.
  connected_ = state == protocol::ConnectionToHost::CONNECTED;
  EnableStatsLogging(connected_);

  logger_->LogSessionStateChange(
      ClientTelemetryLogger::TranslateState(state),
      ClientTelemetryLogger::TranslateError(error));

  if (create_pairing_ && state == protocol::ConnectionToHost::CONNECTED) {
    protocol::PairingRequest request;
    DCHECK(!device_name_.empty());
    request.set_client_name(device_name_);
    client_->host_stub()->RequestPairing(request);
  }

  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&JniClient::OnConnectionState, jni_client_, state, error));
}

void ChromotingJniInstance::OnConnectionReady(bool ready) {
  // We ignore this message, since OnConnectionState tells us the same thing.
}

void ChromotingJniInstance::OnRouteChanged(
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  std::string message = "Channel " + channel_name + " using " +
      protocol::TransportRoute::GetTypeString(route.type) + " connection.";
  __android_log_print(ANDROID_LOG_INFO, "route", "%s", message.c_str());
}

void ChromotingJniInstance::SetCapabilities(const std::string& capabilities) {
  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&JniClient::SetCapabilities, jni_client_, capabilities));
}

void ChromotingJniInstance::SetPairingResponse(
    const protocol::PairingResponse& response) {
  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&JniClient::CommitPairingCredentials, jni_client_,
                            client_auth_config_.host_id, response.client_id(),
                            response.shared_secret()));
}

void ChromotingJniInstance::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&JniClient::HandleExtensionMessage, jni_client_,
                            message.type(), message.data()));
}

void ChromotingJniInstance::SetDesktopSize(const webrtc::DesktopSize& size,
                                           const webrtc::DesktopVector& dpi) {
  // JniFrameConsumer get size from the frames and it doesn't use DPI, so this
  // call can be ignored.
}

protocol::ClipboardStub* ChromotingJniInstance::GetClipboardStub() {
  return this;
}

protocol::CursorShapeStub* ChromotingJniInstance::GetCursorShapeStub() {
  return cursor_shape_stub_.get();
}

void ChromotingJniInstance::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

base::WeakPtr<ChromotingJniInstance> ChromotingJniInstance::GetWeakPtr() {
  return weak_ptr_;
}

void ChromotingJniInstance::ConnectToHostOnNetworkThread() {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  client_context_.reset(new ClientContext(jni_runtime_->network_task_runner()));
  client_context_->Start();

  perf_tracker_.reset(new protocol::PerformanceTracker());

  video_renderer_->Initialize(*client_context_,
                              perf_tracker_.get());

  if (!audio_player_) {
    audio_player_.reset(new AudioPlayerAndroid());
  }

  logger_.reset(new ClientTelemetryLogger(jni_runtime_->GetLogWriter(),
                                          ChromotingEvent::Mode::ME2ME));
  logger_->SetHostInfo(
      connection_info_.host_version,
      ChromotingEvent::ParseOsFromString(connection_info_.host_os),
      connection_info_.host_os_version);

  client_.reset(new ChromotingClient(client_context_.get(), this,
                                     video_renderer_.get(),
                                     audio_player_->GetWeakPtr()));

  signaling_.reset(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             jni_runtime_->url_requester(), xmpp_config_));

  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          signaling_.get(),
          base::MakeUnique<protocol::ChromiumPortAllocatorFactory>(),
          base::MakeUnique<ChromiumUrlRequestFactory>(
              jni_runtime_->url_requester()),
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

void ChromotingJniInstance::SetDeviceName(const std::string& device_name) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SetDeviceName,
                              GetWeakPtr(), device_name));
    return;
  }

  device_name_ = device_name;
}

void ChromotingJniInstance::SendKeyEventInternal(int usb_key_code,
                                                 bool key_down) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SendKeyEventInternal,
                              GetWeakPtr(), usb_key_code, key_down));
    return;
  }

  protocol::KeyEvent event;
  event.set_usb_keycode(usb_key_code);
  event.set_pressed(key_down);
  client_->input_stub()->InjectKeyEvent(event);
}

void ChromotingJniInstance::EnableStatsLogging(bool enabled) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  if (enabled && !stats_logging_enabled_) {
    jni_runtime_->network_task_runner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::LogPerfStats, GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));
  }
  stats_logging_enabled_ = enabled;
}

void ChromotingJniInstance::LogPerfStats() {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  if (!stats_logging_enabled_)
    return;

  __android_log_print(
      ANDROID_LOG_INFO, "stats",
      "Bandwidth:%.0f FrameRate:%.1f;"
      " (Avg, Max) Capture:%.1f, %" PRId64 " Encode:%.1f, %" PRId64
      " Decode:%.1f, %" PRId64 " Render:%.1f, %" PRId64 " RTL:%.0f, %" PRId64,
      perf_tracker_->video_bandwidth(), perf_tracker_->video_frame_rate(),
      perf_tracker_->video_capture_ms().Average(),
      perf_tracker_->video_capture_ms().Max(),
      perf_tracker_->video_encode_ms().Average(),
      perf_tracker_->video_encode_ms().Max(),
      perf_tracker_->video_decode_ms().Average(),
      perf_tracker_->video_decode_ms().Max(),
      perf_tracker_->video_paint_ms().Average(),
      perf_tracker_->video_paint_ms().Max(),
      perf_tracker_->round_trip_ms().Average(),
      perf_tracker_->round_trip_ms().Max());

  logger_->LogStatistics(perf_tracker_.get());

  jni_runtime_->network_task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromotingJniInstance::LogPerfStats, GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));
}

void ChromotingJniInstance::ReleaseResources() {
  logger_.reset();

  // |client_| must be torn down before |signaling_|.
  client_.reset();
  audio_player_.reset();
  video_renderer_.reset();
  signaling_.reset();
  perf_tracker_.reset();
  client_context_.reset();
  cursor_shape_stub_.reset();

  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace remoting
