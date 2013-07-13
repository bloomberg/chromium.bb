// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_instance.h"

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "net/android/net_jni_registrar.h"
#include "remoting/base/url_request_context.h"
#include "remoting/client/audio_player.h"
#include "remoting/protocol/libjingle_transport_factory.h"

namespace remoting {

// static
ChromotingJNIInstance* ChromotingJNIInstance::GetInstance() {
  return Singleton<ChromotingJNIInstance>::get();
}

ChromotingJNIInstance::ChromotingJNIInstance()
    : connected_(false) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // The base and networks stacks must be registered with JNI in order to work
  // on Android. An AtExitManager cleans this up at world's end.
  collector_.reset(new base::AtExitManager());
  base::android::RegisterJni(env);
  net::android::RegisterJni(env);

  // On Android, the UI thread is managed by Java, so we need to attach and
  // start a special type of message loop to allow Chromium code to run tasks.
  LOG(INFO) << "Starting main message loop";
  ui_loop_.reset(new base::MessageLoopForUI());
  ui_loop_->Start();

  LOG(INFO) << "Spawning additional threads";
  // TODO(solb) Stop pretending to control the managed UI thread's lifetime.
  ui_task_runner_ = new AutoThreadTaskRunner(ui_loop_->message_loop_proxy(),
                                             base::MessageLoop::QuitClosure());
  network_task_runner_ = AutoThread::CreateWithType("native_net",
                                                    ui_task_runner_,
                                                    base::MessageLoop::TYPE_IO);
  display_task_runner_ = AutoThread::Create("native_disp",
                                            ui_task_runner_);

  url_requester_ = new URLRequestContextGetter(ui_task_runner_,
                                               network_task_runner_);

  class_ = static_cast<jclass>(env->NewGlobalRef(env->FindClass(JAVA_CLASS)));
}

ChromotingJNIInstance::~ChromotingJNIInstance() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!connected_);

  JNIEnv* env = base::android::AttachCurrentThread();
  env->DeleteGlobalRef(class_);
  // TODO(solb): crbug.com/259594 Detach all threads from JVM here.
}

void ChromotingJNIInstance::ConnectToHost(const char* username,
                                          const char* auth_token,
                                          const char* host_jid,
                                          const char* host_id,
                                          const char* host_pubkey) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!connected_);
  connected_ = true;

  username_ = username;
  auth_token_ = auth_token;
  host_jid_ = host_jid;
  host_id_ = host_id;
  host_pubkey_ = host_pubkey;

  display_task_runner_->PostTask(FROM_HERE, base::Bind(
      &ChromotingJNIInstance::ConnectToHostOnDisplayThread,
      base::Unretained(this)));
}

void ChromotingJNIInstance::DisconnectFromHost() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(connected_);
  connected_ = false;

  network_task_runner_->PostTask(FROM_HERE, base::Bind(
      &ChromotingJNIInstance::DisconnectFromHostOnNetworkThread,
      base::Unretained(this)));
}

void ChromotingJNIInstance::ProvideSecret(const char* pin) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!pin_callback_.is_null());

  // We invoke the string constructor to ensure |pin| gets copied *before* the
  // asynchronous run, since Java might want it back as soon as we return.
  network_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(pin_callback_, std::string(pin)));
}

void ChromotingJNIInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(
        &ChromotingJNIInstance::OnConnectionState,
        base::Unretained(this),
        state,
        error));
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  env->CallStaticVoidMethod(
    class_,
    env->GetStaticMethodID(class_, "reportConnectionStatus", "(II)V"),
    state,
    error);
}

void ChromotingJNIInstance::OnConnectionReady(bool ready) {
  // We ignore this message, since OnConnectionState() tells us the same thing.
}

void ChromotingJNIInstance::SetCapabilities(const std::string& capabilities) {}

void ChromotingJNIInstance::SetPairingResponse(
    const protocol::PairingResponse& response) {
  NOTIMPLEMENTED();
}

protocol::ClipboardStub* ChromotingJNIInstance::GetClipboardStub() {
  NOTIMPLEMENTED();
  return NULL;
}

protocol::CursorShapeStub* ChromotingJNIInstance::GetCursorShapeStub() {
  NOTIMPLEMENTED();
  return NULL;
}

scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
    ChromotingJNIInstance::GetTokenFetcher(const std::string& host_public_key) {
  // Return null to indicate that third-party authentication is unsupported.
  return scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>();
}

void ChromotingJNIInstance::ConnectToHostOnDisplayThread() {
  DCHECK(display_task_runner_->BelongsToCurrentThread());

  if (!frame_consumer_.get()) {
    frame_consumer_ = new FrameConsumerProxy(display_task_runner_);
    // TODO(solb) Instantiate some FrameConsumer implementation and attach it.
  }

  network_task_runner_->PostTask(FROM_HERE, base::Bind(
      &ChromotingJNIInstance::ConnectToHostOnNetworkThread,
      base::Unretained(this)));
}

void ChromotingJNIInstance::ConnectToHostOnNetworkThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  client_config_.reset(new ClientConfig());
  client_config_->host_jid = host_jid_;
  client_config_->host_public_key = host_pubkey_;

  client_config_->fetch_secret_callback = base::Bind(
      &ChromotingJNIInstance::FetchSecret,
      base::Unretained(this));
  client_config_->authentication_tag = host_id_;

  // TODO(solb) Move these hardcoded values elsewhere:
  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_hmac"));
  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_plain"));

  client_context_.reset(new ClientContext(network_task_runner_.get()));
  client_context_->Start();

  connection_.reset(new protocol::ConnectionToHost(true));

  client_.reset(new ChromotingClient(*client_config_,
                                     client_context_.get(),
                                     connection_.get(),
                                     this,
                                     frame_consumer_,
                                     scoped_ptr<AudioPlayer>()));

  signaling_config_.reset(new XmppSignalStrategy::XmppServerConfig());
  signaling_config_->host = CHAT_SERVER;
  signaling_config_->port = CHAT_PORT;
  signaling_config_->use_tls = CHAT_USE_TLS;

  signaling_.reset(new XmppSignalStrategy(url_requester_,
                                          username_,
                                          auth_token_,
                                          CHAT_AUTH_METHOD,
                                          *signaling_config_));

  network_settings_.reset(new NetworkSettings(
      NetworkSettings::NAT_TRAVERSAL_OUTGOING));
  scoped_ptr<protocol::TransportFactory> fact(
      protocol::LibjingleTransportFactory::Create(*network_settings_,
                                                  url_requester_));

  client_->Start(signaling_.get(), fact.Pass());
}

void ChromotingJNIInstance::DisconnectFromHostOnNetworkThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  username_ = "";
  auth_token_ = "";
  host_jid_ = "";
  host_id_ = "";
  host_pubkey_ = "";

  // |client_| must be torn down before |signaling_|.
  pin_callback_.Reset();
  client_.reset();
  connection_.reset();
  client_context_.reset();
  client_config_.reset();
  signaling_.reset();
  signaling_config_.reset();
  network_settings_.reset();
}

void ChromotingJNIInstance::FetchSecret(
    bool pairable,
    const protocol::SecretFetchedCallback& callback) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(
        &ChromotingJNIInstance::FetchSecret,
        base::Unretained(this),
        pairable,
        callback));
    return;
  }

  pin_callback_ = callback;
  JNIEnv* env = base::android::AttachCurrentThread();
  env->CallStaticVoidMethod(
      class_,
      env->GetStaticMethodID(class_, "displayAuthenticationPrompt", "()V"));
}

}  // namespace remoting
