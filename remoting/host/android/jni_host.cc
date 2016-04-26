// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/android/jni_host.h"

#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "jni/Host_jni.h"
#include "net/base/url_util.h"
#include "net/socket/ssl_server_socket.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_host.h"
#include "remoting/host/service_urls.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

using base::android::ConvertJavaStringToUTF8;

namespace remoting {

JniHost::JniHost() : weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();

  base::CommandLine::Init(0, nullptr);

  // Enable support for SSL server sockets, which must be done while still
  // single-threaded.
  net::EnableSSLServerSockets();

  if (!base::MessageLoop::current()) {
    HOST_LOG << "Starting main message loop";

    // On Android, the UI thread is managed by Java, so we need to attach and
    // start a special type of message loop to allow Chromium code to run tasks.
    ui_loop_.reset(new base::MessageLoopForUI());
    ui_loop_->Start();
  } else {
    HOST_LOG << "Using existing main message loop";
    ui_loop_.reset(base::MessageLoopForUI::current());
  }

  factory_.reset(new It2MeHostFactory());
  host_context_ =
      ChromotingHostContext::Create(new remoting::AutoThreadTaskRunner(
          ui_loop_->task_runner(), base::Bind(&base::DoNothing)));

  const ServiceUrls* service_urls = ServiceUrls::GetInstance();
  const bool xmpp_server_valid = net::ParseHostAndPort(
      service_urls->xmpp_server_address(), &xmpp_server_config_.host,
      &xmpp_server_config_.port);
  DCHECK(xmpp_server_valid);

  xmpp_server_config_.use_tls = service_urls->xmpp_server_use_tls();
}

JniHost::~JniHost() {}

// static
bool JniHost::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void JniHost::Destroy(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  delete this;
}

void JniHost::Connect(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& caller,
                      const JavaParamRef<jstring>& user_name,
                      const JavaParamRef<jstring>& auth_token) {
  if (it2me_host_) {
    LOG(ERROR) << "Connect: already connected.";
    return;
  }

  XmppSignalStrategy::XmppServerConfig xmpp_config = xmpp_server_config_;
  xmpp_config.username = ConvertJavaStringToUTF8(user_name);
  xmpp_config.auth_token = ConvertJavaStringToUTF8(auth_token);

  std::string directory_bot_jid =
      ServiceUrls::GetInstance()->directory_bot_jid();

  // Create the It2Me host and start connecting.
  it2me_host_ = factory_->CreateIt2MeHost(host_context_->Copy(), weak_ptr_,
                                          xmpp_config, directory_bot_jid);
  it2me_host_->Connect();
}

void JniHost::Disconnect(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& caller) {}

void JniHost::OnClientAuthenticated(const std::string& client_username) {
  HOST_LOG << "OnClientAuthenticated: " << client_username;
}

void JniHost::OnStoreAccessCode(const std::string& access_code,
                                base::TimeDelta access_code_lifetime) {
  HOST_LOG << "OnStoreAccessCode: " << access_code << ", "
           << access_code_lifetime.InSeconds();
}

void JniHost::OnNatPolicyChanged(bool nat_traversal_enabled) {
  HOST_LOG << "OnNatPolicyChanged: " << nat_traversal_enabled;
}

void JniHost::OnStateChanged(It2MeHostState state,
                             const std::string& error_message) {
  HOST_LOG << "OnStateChanged: " << state;
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new JniHost());
}

}  // namespace remoting
