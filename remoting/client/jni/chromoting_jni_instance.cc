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
    : username_cstr_(NULL),
      auth_token_cstr_(NULL),
      host_jid_cstr_(NULL),
      host_id_cstr_(NULL),
      host_pubkey_cstr_(NULL),
      pin_cstr_(NULL) {
  JNIEnv* env = base::android::AttachCurrentThread();

  collector_.reset(new base::AtExitManager());
  base::android::RegisterJni(env);
  net::android::RegisterJni(env);

  LOG(INFO) << "starting main message loop";
  ui_loop_.reset(new base::MessageLoopForUI());
  ui_loop_->Start();

  LOG(INFO) << "spawning additional threads";
  ui_runner_ = new AutoThreadTaskRunner(ui_loop_->message_loop_proxy(),
                                        base::MessageLoop::QuitClosure());
  net_runner_ = AutoThread::CreateWithType("native_net",
                                           ui_runner_,
                                           base::MessageLoop::TYPE_IO);
  disp_runner_ = AutoThread::CreateWithType("native_disp",
                                            ui_runner_,
                                            base::MessageLoop::TYPE_DEFAULT);

  url_requester_ = new URLRequestContextGetter(ui_runner_, net_runner_);

  class_ = static_cast<jclass>(env->NewGlobalRef(env->FindClass(JAVA_CLASS)));
}

ChromotingJNIInstance::~ChromotingJNIInstance() {
  JNIEnv* env = base::android::AttachCurrentThread();
  env->DeleteGlobalRef(class_);
  // TODO(solb) detach all threads from JVM
}

void ChromotingJNIInstance::ConnectToHost(jstring username,
                                          jstring auth_token,
                                          jstring host_jid,
                                          jstring host_id,
                                          jstring host_pubkey) {
  JNIEnv* env = base::android::AttachCurrentThread();

  username_jstr_ = static_cast<jstring>(env->NewGlobalRef(username));
  auth_token_jstr_ = static_cast<jstring>(env->NewGlobalRef(auth_token));
  host_jid_jstr_ = static_cast<jstring>(env->NewGlobalRef(host_jid));
  host_id_jstr_ = static_cast<jstring>(env->NewGlobalRef(host_id));
  host_pubkey_jstr_ = static_cast<jstring>(env->NewGlobalRef(host_pubkey));

  username_cstr_ = env->GetStringUTFChars(username_jstr_, NULL);
  auth_token_cstr_ = env->GetStringUTFChars(auth_token_jstr_, NULL);
  host_jid_cstr_ = env->GetStringUTFChars(host_jid_jstr_, NULL);
  host_id_cstr_ = env->GetStringUTFChars(host_id_jstr_, NULL);
  host_pubkey_cstr_ = env->GetStringUTFChars(host_pubkey_jstr_, NULL);

  // We're a singleton, so Unretained is safe here.
  disp_runner_->PostTask(FROM_HERE, base::Bind(
      &ChromotingJNIInstance::ConnectToHostOnDisplayThread,
      base::Unretained(this)));
}

void ChromotingJNIInstance::DisconnectFromHost() {
  JNIEnv* env = base::android::AttachCurrentThread();

  env->ReleaseStringUTFChars(username_jstr_, username_cstr_);
  env->ReleaseStringUTFChars(auth_token_jstr_, auth_token_cstr_);
  env->ReleaseStringUTFChars(host_jid_jstr_, host_jid_cstr_);
  env->ReleaseStringUTFChars(host_id_jstr_, host_id_cstr_);
  env->ReleaseStringUTFChars(host_pubkey_jstr_, host_pubkey_cstr_);

  username_cstr_ = NULL;
  auth_token_cstr_ = NULL;
  host_jid_cstr_ = NULL;
  host_id_cstr_ = NULL;
  host_pubkey_cstr_ = NULL;

  env->DeleteGlobalRef(username_jstr_);
  env->DeleteGlobalRef(auth_token_jstr_);
  env->DeleteGlobalRef(host_jid_jstr_);
  env->DeleteGlobalRef(host_id_jstr_);
  env->DeleteGlobalRef(host_pubkey_jstr_);

  if (pin_cstr_) {
    // AuthenticatedWithPin() has been called.
    env->ReleaseStringUTFChars(pin_jstr_, pin_cstr_);
    pin_cstr_ = NULL;
    env->DeleteGlobalRef(pin_jstr_);
  }

  // We're a singleton, so Unretained is safe here.
  net_runner_->PostTask(FROM_HERE, base::Bind(
      &ChromotingJNIInstance::DisconnectFromHostOnNetworkThread,
      base::Unretained(this)));
}

void ChromotingJNIInstance::AuthenticateWithPin(jstring pin) {
  JNIEnv* env = base::android::AttachCurrentThread();

  pin_jstr_ = static_cast<jstring>(env->NewGlobalRef(pin));
  pin_cstr_ = env->GetStringUTFChars(pin_jstr_, NULL);

  net_runner_->PostTask(FROM_HERE, base::Bind(announce_secret_, pin_cstr_));
}

void ChromotingJNIInstance::FetchSecret(
    bool pairable,
    const protocol::SecretFetchedCallback& callback_encore) {
  // All our work must be done on the UI thread.
  if (!ui_runner_->BelongsToCurrentThread()) {
    // We're a singleton, so Unretained is safe here.
    ui_runner_->PostTask(FROM_HERE, base::Bind(
        &ChromotingJNIInstance::FetchSecret,
        base::Unretained(this),
        pairable,
        callback_encore));
    return;
  }

  announce_secret_ = callback_encore;
  JNIEnv* env = base::android::AttachCurrentThread();
  env->CallStaticVoidMethod(
      class_,
      env->GetStaticMethodID(class_, "displayAuthenticationPrompt", "()V"));
}

void ChromotingJNIInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  // All our work must be done on the UI thread.
  if (!ui_runner_->BelongsToCurrentThread()) {
    // We're a singleton, so Unretained is safe here.
    ui_runner_->PostTask(FROM_HERE, base::Bind(
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
}

void ChromotingJNIInstance::SetCapabilities(const std::string& capabilities) {}

void ChromotingJNIInstance::SetPairingResponse(
    const protocol::PairingResponse& response) {}

protocol::ClipboardStub* ChromotingJNIInstance::GetClipboardStub() {
  NOTIMPLEMENTED();
  return NULL;
}

protocol::CursorShapeStub* ChromotingJNIInstance::GetCursorShapeStub() {
  NOTIMPLEMENTED();
  return NULL;
}

// We don't use NOTIMPLEMENTED() here because NegotiatingClientAuthenticator
// calls this even if it doesn't use the configuration method, and we don't
// want to print an error on every run.
scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
    ChromotingJNIInstance::GetTokenFetcher(const std::string& host_public_key) {
  LOG(INFO) << "ChromotingJNIInstance::GetTokenFetcher(...) [unimplemented]";
  return scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>();
}

void ChromotingJNIInstance::ConnectToHostOnDisplayThread() {
  DCHECK(disp_runner_->BelongsToCurrentThread());

  if (!frames_.get()) {
    frames_ = new FrameConsumerProxy(disp_runner_);
    // TODO(solb) Instantiate some FrameConsumer implementation and attach it.
  }

  // We're a singleton, so Unretained is safe here.
  net_runner_->PostTask(FROM_HERE, base::Bind(
      &ChromotingJNIInstance::ConnectToHostOnNetworkThread,
      base::Unretained(this)));
}

void ChromotingJNIInstance::ConnectToHostOnNetworkThread() {
  DCHECK(net_runner_->BelongsToCurrentThread());

  client_config_.reset(new ClientConfig());
  client_config_->host_jid = host_jid_cstr_;
  client_config_->host_public_key = host_pubkey_cstr_;
  // We're a singleton, so Unretained is safe here.
  client_config_->fetch_secret_callback = base::Bind(
      &ChromotingJNIInstance::FetchSecret,
      base::Unretained(this));
  client_config_->authentication_tag = host_id_cstr_;

  // TODO(solb) Move these hardcoded values elsewhere:
  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_pair"));
  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_hmac"));
  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_plain"));

  client_context_.reset(new ClientContext(net_runner_.get()));
  client_context_->Start();

  connection_.reset(new protocol::ConnectionToHost(true));

  client_.reset(new ChromotingClient(*client_config_,
                                     client_context_.get(),
                                     connection_.get(),
                                     this,
                                     frames_,
                                     scoped_ptr<AudioPlayer>()));

  chat_config_.reset(new XmppSignalStrategy::XmppServerConfig());
  chat_config_->host = CHAT_SERVER;
  chat_config_->port = CHAT_PORT;
  chat_config_->use_tls = CHAT_USE_TLS;

  chat_.reset(new XmppSignalStrategy(url_requester_,
                                     username_cstr_,
                                     auth_token_cstr_,
                                     CHAT_AUTH_METHOD,
                                     *chat_config_));

  netset_.reset(new NetworkSettings(NetworkSettings::NAT_TRAVERSAL_OUTGOING));
  scoped_ptr<protocol::TransportFactory> fact(
      protocol::LibjingleTransportFactory::Create(*netset_, url_requester_));

  client_->Start(chat_.get(), fact.Pass());
}

void ChromotingJNIInstance::DisconnectFromHostOnNetworkThread() {
  DCHECK(net_runner_->BelongsToCurrentThread());

  client_.reset();
  connection_.reset();
  client_context_.reset();
  client_config_.reset();
  chat_.reset();  // This object must outlive client_.
  chat_config_.reset();  // TODO(solb) Restructure to reuse between sessions.
  netset_.reset();
}

}  // namespace remoting
