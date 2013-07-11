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

namespace remoting {

ChromotingJNIInstance* ChromotingJNIInstance::GetInstance() {
  return Singleton<ChromotingJNIInstance>::get();
}

// For now, this just gives us access to the strings supplied from Java.
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

  // TODO(solb) Initiate connection from here.
}

// For the moment, this only releases the Java string references.
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
}

void ChromotingJNIInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {}

void ChromotingJNIInstance::OnConnectionReady(bool ready) {}

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

// This currently only spins up the threads.
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

}  // namespace remoting
