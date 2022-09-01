// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"

class WebAuthnBrowserBridge {
 public:
  WebAuthnBrowserBridge(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jbridge);

  WebAuthnBrowserBridge(const WebAuthnBrowserBridge&) = delete;
  WebAuthnBrowserBridge& operator=(const WebAuthnBrowserBridge&) = delete;

  ~WebAuthnBrowserBridge();

  void OnCredentialsDetailsListReceived(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jobjectArray>& credentials,
      const base::android::JavaParamRef<jobject>& jframe_host,
      const base::android::JavaParamRef<jobject>& jcallback) const;

 private:
  // Java object that owns this WebAuthnBrowserBridge.
  base::android::ScopedJavaGlobalRef<jobject> owner_;
};

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_
