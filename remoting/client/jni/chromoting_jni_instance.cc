// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_instance.h"

#include <android/log.h>

#include "base/bind.h"
#include "base/logging.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/jni/android_keymap.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/jingle_glue/chromium_port_allocator.h"
#include "remoting/jingle_glue/chromium_socket_factory.h"
#include "remoting/jingle_glue/network_settings.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/libjingle_transport_factory.h"

namespace remoting {

namespace {

// TODO(solb) Move into location shared with client plugin.
const char* const kXmppServer = "talk.google.com";
const int kXmppPort = 5222;
const bool kXmppUseTls = true;

// Interval at which to log performance statistics, if enabled.
const int kPerfStatsIntervalMs = 10000;

}

ChromotingJniInstance::ChromotingJniInstance(ChromotingJniRuntime* jni_runtime,
                                             const char* username,
                                             const char* auth_token,
                                             const char* host_jid,
                                             const char* host_id,
                                             const char* host_pubkey,
                                             const char* pairing_id,
                                             const char* pairing_secret)
    : jni_runtime_(jni_runtime),
      host_id_(host_id),
      create_pairing_(false),
      stats_logging_enabled_(false) {
  DCHECK(jni_runtime_->ui_task_runner()->BelongsToCurrentThread());

  // Intialize XMPP config.
  xmpp_config_.host = kXmppServer;
  xmpp_config_.port = kXmppPort;
  xmpp_config_.use_tls = kXmppUseTls;
  xmpp_config_.username = username;
  xmpp_config_.auth_token = auth_token;
  xmpp_config_.auth_service = "oauth2";

  // Initialize ClientConfig.
  client_config_.host_jid = host_jid;
  client_config_.host_public_key = host_pubkey;

  client_config_.fetch_secret_callback =
      base::Bind(&ChromotingJniInstance::FetchSecret, this);
  client_config_.authentication_tag = host_id_;

  client_config_.client_pairing_id = pairing_id;
  client_config_.client_paired_secret = pairing_secret;

  client_config_.authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_pair"));
  client_config_.authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_hmac"));
  client_config_.authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_plain"));

  // Post a task to start connection
  jni_runtime_->display_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniInstance::ConnectToHostOnDisplayThread,
                 this));
}

ChromotingJniInstance::~ChromotingJniInstance() {}

void ChromotingJniInstance::Cleanup() {
  if (!jni_runtime_->display_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->display_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::Cleanup, this));
    return;
  }

  // This must be destroyed on the display thread before the producer is gone.
  view_.reset();

  // The weak pointers must be invalidated on the same thread they were used.
  view_weak_factory_->InvalidateWeakPtrs();

  jni_runtime_->network_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniInstance::DisconnectFromHostOnNetworkThread,
                 this));
}

void ChromotingJniInstance::ProvideSecret(const std::string& pin,
                                          bool create_pairing) {
  DCHECK(jni_runtime_->ui_task_runner()->BelongsToCurrentThread());
  DCHECK(!pin_callback_.is_null());

  create_pairing_ = create_pairing;

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

void ChromotingJniInstance::PerformMouseAction(
    int x, int y,
    protocol::MouseEvent_MouseButton button,
    bool button_down) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::PerformMouseAction,
                              this, x, y, button, button_down));
    return;
  }

  protocol::MouseEvent action;
  action.set_x(x);
  action.set_y(y);
  action.set_button(button);
  if (button != protocol::MouseEvent::BUTTON_UNDEFINED)
    action.set_button_down(button_down);

  connection_->input_stub()->InjectMouseEvent(action);
}

void ChromotingJniInstance::PerformMouseWheelDeltaAction(int delta_x,
                                                         int delta_y) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::PerformMouseWheelDeltaAction, this,
                   delta_x, delta_y));
    return;
  }

  protocol::MouseEvent action;
  action.set_wheel_delta_x(delta_x);
  action.set_wheel_delta_y(delta_y);
  connection_->input_stub()->InjectMouseEvent(action);
}

void ChromotingJniInstance::PerformKeyboardAction(int key_code, bool key_down) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::PerformKeyboardAction,
                              this, key_code, key_down));
    return;
  }

  uint32 usb_code = AndroidKeycodeToUsbKeycode(key_code);
  if (usb_code) {
    protocol::KeyEvent action;
    action.set_usb_keycode(usb_code);
    action.set_pressed(key_down);
    connection_->input_stub()->InjectKeyEvent(action);
  } else {
    LOG(WARNING) << "Ignoring unknown keycode: " << key_code;
  }
}

void ChromotingJniInstance::RecordPaintTime(int64 paint_time_ms) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniInstance::RecordPaintTime, this,
                              paint_time_ms));
    return;
  }

  if (stats_logging_enabled_)
    video_renderer_->GetStats()->video_paint_ms()->Record(paint_time_ms);
}

void ChromotingJniInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  EnableStatsLogging(state == protocol::ConnectionToHost::CONNECTED);

  if (create_pairing_ && state == protocol::ConnectionToHost::CONNECTED) {
    protocol::PairingRequest request;
    request.set_client_name("Android");
    connection_->host_stub()->RequestPairing(request);
  }

  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniRuntime::ReportConnectionStatus,
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
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

protocol::ClipboardStub* ChromotingJniInstance::GetClipboardStub() {
  return this;
}

protocol::CursorShapeStub* ChromotingJniInstance::GetCursorShapeStub() {
  return this;
}

scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
    ChromotingJniInstance::GetTokenFetcher(const std::string& host_public_key) {
  // Return null to indicate that third-party authentication is unsupported.
  return scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>();
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

void ChromotingJniInstance::ConnectToHostOnDisplayThread() {
  DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());

  view_.reset(new JniFrameConsumer(jni_runtime_, this));
  view_weak_factory_.reset(new base::WeakPtrFactory<JniFrameConsumer>(
      view_.get()));
  frame_consumer_ = new FrameConsumerProxy(jni_runtime_->display_task_runner(),
                                           view_weak_factory_->GetWeakPtr());

  jni_runtime_->network_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniInstance::ConnectToHostOnNetworkThread,
                 this));
}

void ChromotingJniInstance::ConnectToHostOnNetworkThread() {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  client_context_.reset(new ClientContext(
      jni_runtime_->network_task_runner().get()));
  client_context_->Start();

  connection_.reset(new protocol::ConnectionToHost(true));

  SoftwareVideoRenderer* renderer =
      new SoftwareVideoRenderer(client_context_->main_task_runner(),
                                client_context_->decode_task_runner(),
                                frame_consumer_);
  view_->set_frame_producer(renderer);
  video_renderer_.reset(renderer);

  client_.reset(new ChromotingClient(
      client_config_, client_context_.get(), connection_.get(),
      this, video_renderer_.get(), scoped_ptr<AudioPlayer>()));


  signaling_.reset(new XmppSignalStrategy(
      net::ClientSocketFactory::GetDefaultFactory(),
      jni_runtime_->url_requester(), xmpp_config_));

  NetworkSettings network_settings(NetworkSettings::NAT_TRAVERSAL_ENABLED);

  // Use Chrome's network stack to allocate ports for peer-to-peer channels.
  scoped_ptr<ChromiumPortAllocator> port_allocator(
      ChromiumPortAllocator::Create(jni_runtime_->url_requester(),
                                    network_settings));

  scoped_ptr<protocol::TransportFactory> transport_factory(
      new protocol::LibjingleTransportFactory(
          signaling_.get(),
          port_allocator.PassAs<cricket::HttpPortAllocatorBase>(),
          network_settings));

  client_->Start(signaling_.get(), transport_factory.Pass());
}

void ChromotingJniInstance::DisconnectFromHostOnNetworkThread() {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  host_id_.clear();

  stats_logging_enabled_ = false;

  // |client_| must be torn down before |signaling_|.
  connection_.reset();
  client_.reset();
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

  if (!client_config_.client_pairing_id.empty()) {
    // We attempted to connect using an existing pairing that was rejected.
    // Unless we forget about the stale credentials, we'll continue trying them.
    jni_runtime_->CommitPairingCredentials(host_id_, "", "");
  }

  pin_callback_ = callback;
  jni_runtime_->DisplayAuthenticationPrompt(pairable);
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

  ChromotingStats* stats = video_renderer_->GetStats();
  __android_log_print(ANDROID_LOG_INFO, "stats",
                      "Bandwidth:%.0f FrameRate:%.1f Capture:%.1f Encode:%.1f "
                      "Decode:%.1f Render:%.1f Latency:%.0f",
                      stats->video_bandwidth()->Rate(),
                      stats->video_frame_rate()->Rate(),
                      stats->video_capture_ms()->Average(),
                      stats->video_encode_ms()->Average(),
                      stats->video_decode_ms()->Average(),
                      stats->video_paint_ms()->Average(),
                      stats->round_trip_ms()->Average());

  jni_runtime_->network_task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromotingJniInstance::LogPerfStats, this),
      base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));
}

}  // namespace remoting
