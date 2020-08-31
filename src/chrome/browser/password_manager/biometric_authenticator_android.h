// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/chrome_biometric_authenticator.h"
#include "components/password_manager/core/browser/biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "ui/android/window_android.h"

// Android implementation of the BiometricAuthenticator interface.
class BiometricAuthenticatorAndroid : public ChromeBiometricAuthenticator {
 public:
  explicit BiometricAuthenticatorAndroid(ui::WindowAndroid* window_android);
  ~BiometricAuthenticatorAndroid() override;

  password_manager::BiometricsAvailability CanAuthenticate() override;
  void Authenticate(const password_manager::UiCredential& credential,
                    AuthenticateCallback callback) override;

 private:
  // This object is an instance of BiometricAuthenticatorBridge, i.e. the Java
  // counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
