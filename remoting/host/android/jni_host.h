// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ANDROID_JNI_HOST_H_
#define REMOTING_HOST_ANDROID_JNI_HOST_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "remoting/host/it2me/it2me_host.h"

namespace remoting {

class ChromotingHostContext;
class It2MeHostFactory;

class JniHost : public It2MeHost::Observer {
 public:
  JniHost(JNIEnv* env, jobject java_host);
  virtual ~JniHost();

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJni(JNIEnv* env);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& caller);

  void Connect(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& caller,
               const base::android::JavaParamRef<jstring>& user_name,
               const base::android::JavaParamRef<jstring>& auth_token);

  void Disconnect(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller);

  // It2MeHost::Observer implementation.
  void OnClientAuthenticated(const std::string& client_username) override;
  void OnStoreAccessCode(const std::string& access_code,
                         base::TimeDelta access_code_lifetime) override;
  void OnNatPolicyChanged(bool nat_traversal_enabled) override;
  void OnStateChanged(It2MeHostState state,
                      const std::string& error_message) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_host_;

  std::unique_ptr<base::MessageLoopForUI> ui_loop_;
  std::unique_ptr<ChromotingHostContext> host_context_;
  std::unique_ptr<It2MeHostFactory> factory_;
  scoped_refptr<It2MeHost> it2me_host_;

  // IT2Me Talk server configuration used by |it2me_host_| to connect.
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;

  base::WeakPtr<JniHost> weak_ptr_;
  base::WeakPtrFactory<JniHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(JniHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_ANDROID_JNI_HOST_H_
