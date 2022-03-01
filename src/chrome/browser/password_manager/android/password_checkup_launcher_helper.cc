// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"

#include "chrome/android/chrome_jni_headers/PasswordCheckupLauncher_jni.h"

// static
void PasswordCheckupLauncherHelper::LaunchCheckupInAccountWithWindowAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& windowAndroid) {
  Java_PasswordCheckupLauncher_launchCheckupInAccountWithWindowAndroid(
      env, checkupUrl, windowAndroid);
}

// static
void PasswordCheckupLauncherHelper::LaunchLocalCheckup(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& windowAndroid) {
  Java_PasswordCheckupLauncher_launchLocalCheckup(env, windowAndroid);
}

// static
void PasswordCheckupLauncherHelper::
    LaunchLocalCheckupFromPhishGuardWarningDialog(
        JNIEnv* env,
        const base::android::JavaRef<jobject>& windowAndroid) {
  Java_PasswordCheckupLauncher_launchLocalCheckupFromPhishGuardWarningDialog(
      env, windowAndroid);
}

// static
void PasswordCheckupLauncherHelper::LaunchCheckupInAccountWithActivity(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& activity) {
  Java_PasswordCheckupLauncher_launchCheckupInAccountWithActivity(
      env, checkupUrl, activity);
}
