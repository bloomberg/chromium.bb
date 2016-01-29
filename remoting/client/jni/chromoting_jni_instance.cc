// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_instance.h"

#include <android/log.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/service_urls.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/client_status_logger.h"
#include "remoting/client/jni/android_keymap.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_frame_consumer.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/client/token_fetcher_proxy.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
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
      host_id_(host_id),
      host_jid_(host_jid),
      flags_(flags),
      create_pairing_(false),
      stats_logging_enabled_(false),
      capabilities_(capabilities),
      weak_factory_(this) {
  DCHECK(jni_runtime_->ui_task_runner()->BelongsToCurrentThread());

  // Initialize XMPP config.
  xmpp_config_.host = kXmppServer;
  xmpp_config_.port = kXmppPort;
  xmpp_config_.use_tls = kXmppUseTls;
  xmpp_config_.username = username;
  xmpp_config_.auth_token = auth_token;

  // Initialize |authenticator_|.
  scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
      token_fetcher(new TokenFetcherProxy(
          base::Bind(&ChromotingJniInstance::FetchThirdPartyToken,
                     weak_factory_.GetWeakPtr()),
          host_pubkey));

  std::vector<protocol::AuthenticationMethod> auth_methods;
  auth_methods.push_back(protocol::AuthenticationMethod::Spake2Pair());
  auth_methods.push_back(protocol::AuthenticationMethod::Spake2(
      protocol::AuthenticationMethod::HMAC_SHA256));
  auth_methods.push_back(protocol::AuthenticationMethod::Spake2(
      protocol::AuthenticationMethod::NONE));
  auth_methods.push_back(protocol::AuthenticationMethod::ThirdParty());

  authenticator_.reset(new protocol::NegotiatingClientAuthenticator(
      pairing_id, pairing_secret, host_id_,
      base::Bind(&ChromotingJniInstance::FetchSecret, this),
      std::move(token_fetcher), auth_methods));

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
  DCHECK(!authenticator_);
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

  host_id_.clear();

  stats_logging_enabled_ = false;

  // |client_| must be torn down before |signaling_|.
  client_.reset();
  client_status_logger_.reset();
  video_renderer_.reset();
  view_.reset();
  authenticator_.reset();
  signaling_.reset();
  perf_tracker_.reset();
  client_context_.reset();
}

void ChromotingJniInstance::FetchThirdPartyToken(
    const GURL& token_url,
    const std::string& client_id,
    const std::string& scope,
    base::WeakPtr<TokenFetcherProxy> token_fetcher_proxy) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!token_fetcher_proxy_.get());

  __android_log_print(ANDROID_LOG_INFO,
                      "ThirdPartyAuth",
                      "Fetching Third Party Token from user.");

  token_fetcher_proxy_ = token_fetcher_proxy;
  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniRuntime::FetchThirdPartyToken,
                 base::Unretained(jni_runtime_),
                 token_url,
                 client_id,
                 scope));
}

void ChromotingJniInstance::HandleOnThirdPartyTokenFetched(
    const std::string& token,
    const std::string& shared_secret) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  __android_log_print(
      ANDROID_LOG_INFO, "ThirdPartyAuth", "Third Party Token Fetched.");

  if (token_fetcher_proxy_.get()) {
    token_fetcher_proxy_->OnTokenFetched(token, shared_secret);
    token_fetcher_proxy_.reset();
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
      FROM_HERE,
      base::Bind(&ChromotingJniRuntime::SetCapabilities,
                 base::Unretained(jni_runtime_),
                 capabilities));
}

void ChromotingJniInstance::SetPairingResponse(
    const protocol::PairingResponse& response) {

  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniRuntime::CommitPairingCredentials,
                 base::Unretained(jni_runtime_),
                 host_id_, response.client_id(), response.shared_secret()));
}

void ChromotingJniInstance::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniRuntime::HandleExtensionMessage,
                 base::Unretained(jni_runtime_),
                 message.type(),
                 message.data()));
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

  client_.reset(new ChromotingClient(
      client_context_.get(), this, video_renderer_.get(), nullptr));

  signaling_.reset(new XmppSignalStrategy(
      net::ClientSocketFactory::GetDefaultFactory(),
      jni_runtime_->url_requester(), xmpp_config_));

  client_status_logger_.reset(
      new ClientStatusLogger(ServerLogEntry::ME2ME,
                             signaling_.get(),
                             ServiceUrls::GetInstance()->directory_bot_jid()));

  protocol::NetworkSettings network_settings(
      protocol::NetworkSettings::NAT_TRAVERSAL_FULL);

  // Use Chrome's network stack to allocate ports for peer-to-peer channels.
  scoped_ptr<protocol::ChromiumPortAllocatorFactory> port_allocator_factory(
      new protocol::ChromiumPortAllocatorFactory(
          jni_runtime_->url_requester()));

  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          signaling_.get(), std::move(port_allocator_factory), network_settings,
          protocol::TransportRole::CLIENT);

  if (flags_.find("useWebrtc") != std::string::npos) {
    VLOG(0) << "Attempting to connect using WebRTC.";
    scoped_ptr<protocol::CandidateSessionConfig> protocol_config =
        protocol::CandidateSessionConfig::CreateEmpty();
    protocol_config->set_webrtc_supported(true);
    protocol_config->set_ice_supported(false);
    client_->set_protocol_config(std::move(protocol_config));
  }

  client_->Start(signaling_.get(), std::move(authenticator_), transport_context,
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
  jni_runtime_->CommitPairingCredentials(host_id_, "", "");

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
      "Bandwidth:%.0f FrameRate:%.1f Capture:%.1f Encode:%.1f "
      "Decode:%.1f Render:%.1f Latency:%.0f",
      perf_tracker_->video_bandwidth(), perf_tracker_->video_frame_rate(),
      perf_tracker_->video_capture_ms(), perf_tracker_->video_encode_ms(),
      perf_tracker_->video_decode_ms(), perf_tracker_->video_paint_ms(),
      perf_tracker_->round_trip_ms());

  client_status_logger_->LogStatistics(perf_tracker_.get());

  jni_runtime_->network_task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromotingJniInstance::LogPerfStats, this),
      base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));
}

}  // namespace remoting
