// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "chrome/android/chrome_jni_headers/PasswordChangeLauncher_jni.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#include "chrome/browser/ui/android/passwords/credential_leak_dialog_view_android.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ui/android/window_android.h"

using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LogLeakDialogTypeAndDismissalReason;

CredentialLeakControllerAndroid::CredentialLeakControllerAndroid(
    password_manager::CredentialLeakType leak_type,
    password_manager::CompromisedSitesCount saved_sites,
    const GURL& origin,
    const base::string16& username,
    ui::WindowAndroid* window_android)
    : leak_type_(leak_type),
      saved_sites_(saved_sites),
      origin_(origin),
      username_(username),
      window_android_(window_android) {}

CredentialLeakControllerAndroid::~CredentialLeakControllerAndroid() = default;

void CredentialLeakControllerAndroid::ShowDialog() {
  dialog_view_.reset(new CredentialLeakDialogViewAndroid(this));
  dialog_view_->Show(window_android_);
}

void CredentialLeakControllerAndroid::OnCancelDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kClickedClose);
  delete this;
}

void CredentialLeakControllerAndroid::OnAcceptDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      ShouldCheckPasswords() ? LeakDialogDismissalReason::kClickedCheckPasswords
                             : LeakDialogDismissalReason::kClickedOk);

  // |window_android_| might be null in tests.
  if (!window_android_) {
    delete this;
    return;
  }

  DCHECK(!(ShouldCheckPasswords() && ShouldShowChangePasswordButton()));
  JNIEnv* env = base::android::AttachCurrentThread();
  if (ShouldCheckPasswords()) {
    if (base::FeatureList::IsEnabled(
            password_manager::features::kPasswordCheck)) {
      PasswordCheckupLauncherHelper::LaunchLocalCheckup(
          env, window_android_->GetJavaObject());
    } else {
      PasswordCheckupLauncherHelper::LaunchCheckupInAccountWithWindowAndroid(
          env,
          base::android::ConvertUTF8ToJavaString(
              env, password_manager::GetPasswordCheckupURL().spec()),
          window_android_->GetJavaObject());
    }
  } else if (ShouldShowChangePasswordButton()) {
    Java_PasswordChangeLauncher_start(
        env, window_android_->GetJavaObject(),
        base::android::ConvertUTF8ToJavaString(env, origin_.spec()),
        base::android::ConvertUTF16ToJavaString(env, username_));
  }

  delete this;
}

void CredentialLeakControllerAndroid::OnCloseDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kNoDirectInteraction);
  delete this;
}

base::string16 CredentialLeakControllerAndroid::GetAcceptButtonLabel() const {
  return password_manager::GetAcceptButtonLabel(leak_type_);
}

base::string16 CredentialLeakControllerAndroid::GetCancelButtonLabel() const {
  return password_manager::GetCancelButtonLabel();
}

base::string16 CredentialLeakControllerAndroid::GetDescription() const {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordCheck)) {
    return password_manager::GetDescriptionWithCount(leak_type_, origin_,
                                                     saved_sites_);
  }
  return password_manager::GetDescription(leak_type_, origin_);
}

base::string16 CredentialLeakControllerAndroid::GetTitle() const {
  return password_manager::GetTitle(leak_type_);
}

bool CredentialLeakControllerAndroid::ShouldCheckPasswords() const {
  return password_manager::ShouldCheckPasswords(leak_type_);
}

bool CredentialLeakControllerAndroid::ShouldShowChangePasswordButton() const {
  return password_manager::ShouldShowChangePasswordButton(leak_type_);
}

bool CredentialLeakControllerAndroid::ShouldShowCancelButton() const {
  return password_manager::ShouldShowCancelButton(leak_type_);
}
