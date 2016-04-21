// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_instance.h"

#include <android/log.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/client/audio_player_android.h"
#include "remoting/client/client_status_logger.h"
#include "remoting/client/jni/android_keymap.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_frame_consumer.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/server_log_entry.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

// TODO(solb) Move into location shared with client plugin.
const char* const kXmppServer = "talk.google.com";
const int kXmppPort = 5222;
const bool kXmppUseTls = true;

const char kDirectoryBotJid[] = "remoting@bot.talk.google.com";

// Interval at which to log performance statistics, if enabled.
const int kPerfStatsIntervalMs = 60000;

}

ChromotingJniInstance::ChromotingJniInstance(ChromotingJniRuntime* jni_runtime,
                                             const std::string& username,
                                             const std::string& auth_token,
                                             const std::string& host_jid,
                                             const std::string& host_id,
                                             const std::string& host_pubkey,
                                             const std::string& pairing_id,
                                             const std::string& pairing_secret,
                                             const std::string& capabilities,
                                             const std::string& flags)
    : jni_runtime_(jni_runtime),
      host_jid_(host_jid),
      flags_(flags),
      capabilities_(capabilities),
      weak_factory_(this) {
  DCHECK(jni_runtime_->ui_task_runner()->BelongsToCurrentThread());

  // Initialize XMPP config.
  xmpp_config_.host = kXmppServer;
  xmpp_config_.port = kXmppPort;
  xmpp_config_.use_tls = kXmppUseTls;
  xmpp_config_.username = username;
  xmpp_config_.auth_token = auth_token;

  client_auth_config_.host_id = host_id;
  client_auth_config_.pairing_client_id = pairing_id;
  client_auth_config_.pairing_secret = pairing_secret;
  client_auth_config_.fetch_secret_callback = base::Bind(
      &ChromotingJniInstance::FetchSecret, weak_factory_.GetWeakPtr());
  client_auth_config_.fetch_third_party_token_callback =
      base::Bind(&ChromotingJniInstance::FetchThirdPartyToken,
                 weak_factory_.GetWeakPtr(), host_pubkey);

  // Post a task to start connection
  jni_runtime_->network_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniInstance::ConnectToHostOnNetworkThread, this));
}

ChromotingJniInstance::~ChromotingJniInstance() {
  // This object is ref-counted, so this dtor can execute on any thread.
  // Ensure that all these objects have been freed already, so they are not
  // destroyed on some random thread.
  DCHECK(!view_);
  DCHECK(!client_context_);
  DCHECK(!video_renderer_);
  DCHECK(!client_);
  DCHECK(!signaling_);
  DCHECK(!client_status_logger_);
}

void ChromotingJniInstance::Disconnect() {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::Disconnect, this));
    return;
  }

  stats_logging_enabled_ = false;

  // |client_| must be torn down before |signaling_|.
  client_.reset();
  client_status_logger_.reset();
  video_renderer_.reset();
  view_.reset();
  signaling_.reset();
  perf_tracker_.reset();
  client_context_.reset();
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
      FROM_HERE, base::Bind(&ChromotingJniRuntime::FetchThirdPartyToken,
                            base::Unretained(jni_runtime_), token_url,
                            host_public_key, scope));
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
  DCHECK(!pin_callback_.is_null());

  create_pairing_ = create_pairing;

  if (create_pairing)
    SetDeviceName(device_name);

  jni_runtime_->network_task_runner()->PostTask(FROM_HERE,
                                                base::Bind(pin_callback_, pin));
}

void ChromotingJniInstance::RedrawDesktop() {
  if (!jni_runtime_->display_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->display_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::RedrawDesktop, this));
    return;
  }

  jni_runtime_->RedrawCanvas();
}

void ChromotingJniInstance::SendMouseEvent(
    int x, int y,
    protocol::MouseEvent_MouseButton button,
    bool button_down) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SendMouseEvent,
                              this, x, y, button, button_down));
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
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::SendMouseWheelEvent, this,
                   delta_x, delta_y));
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
        base::Bind(&ChromotingJniInstance::SendTextEvent, this, text));
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
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::SendTouchEvent, this, touch_event));
    return;
  }

  client_->input_stub()->InjectTouchEvent(touch_event);
}

void ChromotingJniInstance::EnableVideoChannel(bool enable) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::EnableVideoChannel, this, enable));
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
        FROM_HERE,
        base::Bind(
            &ChromotingJniInstance::SendClientMessage, this, type, data));
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

  EnableStatsLogging(state == protocol::ConnectionToHost::CONNECTED);

  client_status_logger_->LogSessionStateChange(state, error);

  if (create_pairing_ && state == protocol::ConnectionToHost::CONNECTED) {
    protocol::PairingRequest request;
    DCHECK(!device_name_.empty());
    request.set_client_name(device_name_);
    client_->host_stub()->RequestPairing(request);
  }

  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniRuntime::OnConnectionState,
                 base::Unretained(jni_runtime_),
                 state,
                 error));
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
      FROM_HERE, base::Bind(&ChromotingJniRuntime::SetCapabilities,
                            base::Unretained(jni_runtime_), capabilities));
}

void ChromotingJniInstance::SetPairingResponse(
    const protocol::PairingResponse& response) {
  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniRuntime::CommitPairingCredentials,
                 base::Unretained(jni_runtime_), client_auth_config_.host_id,
                 response.client_id(), response.shared_secret()));
}

void ChromotingJniInstance::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ChromotingJniRuntime::HandleExtensionMessage,
                            base::Unretained(jni_runtime_), message.type(),
                            message.data()));
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
  return this;
}

void ChromotingJniInstance::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

void ChromotingJniInstance::SetCursorShape(
    const protocol::CursorShapeInfo& shape) {
  if (!jni_runtime_->display_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->display_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::SetCursorShape, this, shape));
    return;
  }

  jni_runtime_->UpdateCursorShape(shape);
}

void ChromotingJniInstance::ConnectToHostOnNetworkThread() {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  client_context_.reset(new ClientContext(jni_runtime_->network_task_runner()));
  client_context_->Start();

  perf_tracker_.reset(new protocol::PerformanceTracker());

  view_.reset(new JniFrameConsumer(jni_runtime_));
  video_renderer_.reset(new SoftwareVideoRenderer(
      client_context_->decode_task_runner(), view_.get(), perf_tracker_.get()));

  client_.reset(
      new ChromotingClient(client_context_.get(), this, video_renderer_.get(),
                           base::WrapUnique(new AudioPlayerAndroid())));

  signaling_.reset(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             jni_runtime_->url_requester(), xmpp_config_));

  client_status_logger_.reset(new ClientStatusLogger(
      ServerLogEntry::ME2ME, signaling_.get(), kDirectoryBotJid));

  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          signaling_.get(),
          base::WrapUnique(new protocol::ChromiumPortAllocatorFactory()),
          base::WrapUnique(
              new ChromiumUrlRequestFactory(jni_runtime_->url_requester())),
          protocol::NetworkSettings(
              protocol::NetworkSettings::NAT_TRAVERSAL_FULL),
          protocol::TransportRole::CLIENT);

#if defined(ENABLE_WEBRTC_REMOTING_CLIENT)
  if (flags_.find("useWebrtc") != std::string::npos) {
    VLOG(0) << "Attempting to connect using WebRTC.";
    std::unique_ptr<protocol::CandidateSessionConfig> protocol_config =
        protocol::CandidateSessionConfig::CreateEmpty();
    protocol_config->set_webrtc_supported(true);
    protocol_config->set_ice_supported(false);
    client_->set_protocol_config(std::move(protocol_config));
  }
#endif  // defined(ENABLE_WEBRTC_REMOTING_CLIENT)
  client_->Start(signaling_.get(), client_auth_config_, transport_context,
                 host_jid_, capabilities_);
}

void ChromotingJniInstance::FetchSecret(
    bool pairable,
    const protocol::SecretFetchedCallback& callback) {
  if (!jni_runtime_->ui_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->ui_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::FetchSecret,
                              this, pairable, callback));
    return;
  }

  // Delete pairing credentials if they exist.
  jni_runtime_->CommitPairingCredentials(client_auth_config_.host_id, "", "");

  pin_callback_ = callback;
  jni_runtime_->DisplayAuthenticationPrompt(pairable);
}

void ChromotingJniInstance::SetDeviceName(const std::string& device_name) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SetDeviceName, this,
                              device_name));
    return;
  }

  device_name_ = device_name;
}

void ChromotingJniInstance::SendKeyEventInternal(int usb_key_code,
                                                 bool key_down) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::SendKeyEventInternal,
                              this, usb_key_code, key_down));
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
        FROM_HERE, base::Bind(&ChromotingJniInstance::LogPerfStats, this),
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

  client_status_logger_->LogStatistics(perf_tracker_.get());

  jni_runtime_->network_task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromotingJniInstance::LogPerfStats, this),
      base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));
}

}  // namespace remoting
