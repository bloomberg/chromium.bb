// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/PasswordChangeLauncher_jni.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#include "chrome/browser/ui/android/passwords/credential_leak_dialog_view_android.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"

using password_manager::CreateDialogTraits;
using password_manager::PasswordChangeSuccessTracker;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogType;
using password_manager::metrics_util::LogLeakDialogTypeAndDismissalReason;

CredentialLeakControllerAndroid::CredentialLeakControllerAndroid(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin,
    const std::u16string& username,
    PasswordChangeSuccessTracker* password_change_success_tracker,
    ui::WindowAndroid* window_android)
    : leak_type_(leak_type),
      origin_(origin),
      username_(username),
      password_change_success_tracker_(password_change_success_tracker),
      window_android_(window_android),
      leak_dialog_traits_(CreateDialogTraits(leak_type)) {}

CredentialLeakControllerAndroid::~CredentialLeakControllerAndroid() = default;

void CredentialLeakControllerAndroid::ShowDialog() {
  dialog_view_ = std::make_unique<CredentialLeakDialogViewAndroid>(this);
  dialog_view_->Show(window_android_);
}

void CredentialLeakControllerAndroid::OnCancelDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kClickedClose);
  delete this;
}

void CredentialLeakControllerAndroid::OnAcceptDialog() {
  LeakDialogType dialog_type = password_manager::GetLeakDialogType(leak_type_);
  LeakDialogDismissalReason dismissal_reason =
      LeakDialogDismissalReason::kClickedOk;
  switch (dialog_type) {
    case LeakDialogType::kChange:
      dismissal_reason = LeakDialogDismissalReason::kClickedOk;
      break;
    case LeakDialogType::kCheckup:
    case LeakDialogType::kCheckupAndChange:
      dismissal_reason = LeakDialogDismissalReason::kClickedCheckPasswords;
      break;
    case LeakDialogType::kChangeAutomatically:
      dismissal_reason =
          LeakDialogDismissalReason::kClickedChangePasswordAutomatically;
      // Register that an automated password change flow was started.
      // |password_change_success_tracker_| might be null in tests.
      if (password_change_success_tracker_) {
        password_change_success_tracker_->OnChangePasswordFlowStarted(
            origin_, base::UTF16ToUTF8(username_),
            PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
            PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);
      }
      break;
  }

  LogLeakDialogTypeAndDismissalReason(dialog_type, dismissal_reason);

  // |window_android_| might be null in tests.
  if (!window_android_) {
    delete this;
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  switch (dialog_type) {
    case LeakDialogType::kChange:
      // No-op.
      break;
    case LeakDialogType::kCheckup:
    case LeakDialogType::kCheckupAndChange:
      PasswordCheckupLauncherHelper::LaunchLocalCheckup(
          env, window_android_->GetJavaObject());
      break;
    case LeakDialogType::kChangeAutomatically:
      Java_PasswordChangeLauncher_start(
          env, window_android_->GetJavaObject(),
          url::GURLAndroid::FromNativeGURL(env, origin_),
          base::android::ConvertUTF16ToJavaString(env, username_),
          /*skip_login=*/true);
      break;
  }

  delete this;
}

void CredentialLeakControllerAndroid::OnCloseDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kNoDirectInteraction);
  delete this;
}

std::u16string CredentialLeakControllerAndroid::GetAcceptButtonLabel() const {
  return leak_dialog_traits_->GetAcceptButtonLabel();
}

std::u16string CredentialLeakControllerAndroid::GetCancelButtonLabel() const {
  return leak_dialog_traits_->GetCancelButtonLabel();
}

std::u16string CredentialLeakControllerAndroid::GetDescription() const {
  return leak_dialog_traits_->GetDescription();
}

std::u16string CredentialLeakControllerAndroid::GetTitle() const {
  return leak_dialog_traits_->GetTitle();
}

bool CredentialLeakControllerAndroid::ShouldShowCancelButton() const {
  return leak_dialog_traits_->ShouldShowCancelButton();
}

bool CredentialLeakControllerAndroid::ShouldShowAutomaticChangePasswordButton()
    const {
  return password_manager::ShouldShowAutomaticChangePasswordButton(leak_type_);
}
