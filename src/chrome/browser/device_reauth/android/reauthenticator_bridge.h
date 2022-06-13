// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/device_reauth/biometric_authenticator.h"

// C++ counterpart of |ReauthenticatorBridge.java|. Used to mediate the
// biometric authentication requests.
class ReauthenticatorBridge {
 public:
  explicit ReauthenticatorBridge(
      const base::android::JavaParamRef<jobject>& java_bridge,
      jint requester);
  ~ReauthenticatorBridge();

  ReauthenticatorBridge(const ReauthenticatorBridge&) = delete;
  ReauthenticatorBridge& operator=(const ReauthenticatorBridge&) = delete;

  // Called by Java to check if authentication can be used.
  bool CanUseAuthentication(JNIEnv* env);

  // Called by Java to start authentication.
  void Reauthenticate(JNIEnv* env);

  // Called when reauthentication is completed.
  void OnReauthenticationCompleted(bool auth_succeeded);

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;

  // The authentication requester.
  device_reauth::BiometricAuthRequester requester_;

  // The authenticator used to trigger a biometric re-auth.
  scoped_refptr<device_reauth::BiometricAuthenticator> authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_
