// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_instance.h"

#include "base/bind.h"
#include "base/logging.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/jni/android_keymap.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/libjingle_transport_factory.h"

// TODO(solb) Move into location shared with client plugin.
const char* const kXmppServer = "talk.google.com";
const int kXmppPort = 5222;
const bool kXmppUseTls = true;

namespace remoting {

ChromotingJniInstance::ChromotingJniInstance(ChromotingJniRuntime* jni_runtime,
                                             const char* username,
                                             const char* auth_token,
                                             const char* host_jid,
                                             const char* host_id,
                                             const char* host_pubkey,
                                             const char* pairing_id,
                                             const char* pairing_secret)
    : jni_runtime_(jni_runtime),
      username_(username),
      auth_token_(auth_token),
      host_jid_(host_jid),
      host_id_(host_id),
      host_pubkey_(host_pubkey),
      pairing_id_(pairing_id),
      pairing_secret_(pairing_secret),
      create_pairing_(false) {
  DCHECK(jni_runtime_->ui_task_runner()->BelongsToCurrentThread());

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
    int x,
    int y,
    protocol::MouseEvent_MouseButton button,
    bool button_down) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::PerformMouseAction,
                   this,
                   x,
                   y,
                   button,
                   button_down));
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

void ChromotingJniInstance::PerformKeyboardAction(int key_code, bool key_down) {
  if (!jni_runtime_->network_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::PerformKeyboardAction,
                   this,
                   key_code,
                   key_down));
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

void ChromotingJniInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  if (create_pairing_ && state == protocol::ConnectionToHost::CONNECTED) {
    LOG(INFO) << "Attempting to pair with host";
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
  // We ignore this message, since OnConnectoinState tells us the same thing.
}

void ChromotingJniInstance::SetCapabilities(const std::string& capabilities) {}

void ChromotingJniInstance::SetPairingResponse(
    const protocol::PairingResponse& response) {
  LOG(INFO) << "Successfully established pairing with host";

  jni_runtime_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniRuntime::CommitPairingCredentials,
                 base::Unretained(jni_runtime_),
                 host_id_,
                 response.client_id(),
                 response.shared_secret()));
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
  NOTIMPLEMENTED();
}

void ChromotingJniInstance::ConnectToHostOnDisplayThread() {
  DCHECK(jni_runtime_->display_task_runner()->BelongsToCurrentThread());

  frame_consumer_ = new FrameConsumerProxy(jni_runtime_->display_task_runner());
  view_.reset(new JniFrameConsumer(jni_runtime_));
  view_weak_factory_.reset(new base::WeakPtrFactory<JniFrameConsumer>(
      view_.get()));
  frame_consumer_->Attach(view_weak_factory_->GetWeakPtr());

  jni_runtime_->network_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniInstance::ConnectToHostOnNetworkThread,
                 this));
}

void ChromotingJniInstance::ConnectToHostOnNetworkThread() {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  client_config_.reset(new ClientConfig());
  client_config_->host_jid = host_jid_;
  client_config_->host_public_key = host_pubkey_;

  client_config_->fetch_secret_callback = base::Bind(
      &ChromotingJniInstance::FetchSecret,
      this);
  client_config_->authentication_tag = host_id_;

  if (!pairing_id_.empty() && !pairing_secret_.empty()) {
    client_config_->client_pairing_id = pairing_id_;
    client_config_->client_paired_secret = pairing_secret_;
    client_config_->authentication_methods.push_back(
        protocol::AuthenticationMethod::FromString("spake2_pair"));
  }

  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_hmac"));
  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_plain"));

  client_context_.reset(new ClientContext(
      jni_runtime_->network_task_runner().get()));
  client_context_->Start();

  connection_.reset(new protocol::ConnectionToHost(true));

  client_.reset(new ChromotingClient(*client_config_,
                                     client_context_.get(),
                                     connection_.get(),
                                     this,
                                     frame_consumer_,
                                     scoped_ptr<AudioPlayer>()));

  view_->set_frame_producer(client_->GetFrameProducer());

  signaling_config_.reset(new XmppSignalStrategy::XmppServerConfig());
  signaling_config_->host = kXmppServer;
  signaling_config_->port = kXmppPort;
  signaling_config_->use_tls = kXmppUseTls;

  signaling_.reset(new XmppSignalStrategy(jni_runtime_->url_requester(),
                                          username_,
                                          auth_token_,
                                          "oauth2",
                                          *signaling_config_));

  network_settings_.reset(new NetworkSettings(
      NetworkSettings::NAT_TRAVERSAL_ENABLED));
  scoped_ptr<protocol::TransportFactory> fact(
      protocol::LibjingleTransportFactory::Create(
          *network_settings_,
          jni_runtime_->url_requester()));

  client_->Start(signaling_.get(), fact.Pass());
}

void ChromotingJniInstance::DisconnectFromHostOnNetworkThread() {
  DCHECK(jni_runtime_->network_task_runner()->BelongsToCurrentThread());

  username_ = "";
  auth_token_ = "";
  host_jid_ = "";
  host_id_ = "";
  host_pubkey_ = "";

  // |client_| must be torn down before |signaling_|.
  connection_.reset();
  client_.reset();
}

void ChromotingJniInstance::FetchSecret(
    bool pairable,
    const protocol::SecretFetchedCallback& callback) {
  if (!jni_runtime_->ui_task_runner()->BelongsToCurrentThread()) {
    jni_runtime_->ui_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::FetchSecret,
                   this,
                   pairable,
                   callback));
    return;
  }

  if (!pairing_id_.empty() || !pairing_secret_.empty()) {
    // We attempted to connect using an existing pairing that was rejected.
    // Unless we forget about the stale credentials, we'll continue trying them.
    LOG(INFO) << "Deleting rejected pairing credentials";
    jni_runtime_->CommitPairingCredentials(host_id_, "", "");
  }

  pin_callback_ = callback;
  jni_runtime_->DisplayAuthenticationPrompt();
}

}  // namespace remoting
