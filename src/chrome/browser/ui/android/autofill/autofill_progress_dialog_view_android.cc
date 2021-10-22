// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <stddef.h>
#include "chrome/browser/android/resource_mapper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/autofill/autofill_progress_dialog_view_android.h"
#include "chrome/browser/ui/android/autofill/internal/jni_headers/AutofillProgressDialogBridge_jni.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"

using base::android::ConvertUTF16ToJavaString;

namespace autofill {

AutofillProgressDialogViewAndroid::AutofillProgressDialogViewAndroid(
    AutofillProgressDialogController* controller)
    : controller_(controller) {}

AutofillProgressDialogViewAndroid::~AutofillProgressDialogViewAndroid() =
    default;

// static
AutofillProgressDialogView* AutofillProgressDialogView::CreateAndShow(
    AutofillProgressDialogController* controller) {
  AutofillProgressDialogViewAndroid* dialog_view =
      new AutofillProgressDialogViewAndroid(controller);
  dialog_view->ShowDialog();
  return dialog_view;
}

void AutofillProgressDialogViewAndroid::Dismiss(
    bool show_confirmation_before_closing) {
  if (show_confirmation_before_closing) {
    ShowConfirmation();
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillProgressDialogBridge_dismiss(env, java_object_);
  } else {
    OnDismissed(env);
  }
}

void AutofillProgressDialogViewAndroid::OnDismissed(JNIEnv* env) {
  controller_->OnDismissed();
  controller_ = nullptr;
  delete this;
}

void AutofillProgressDialogViewAndroid::ShowDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android =
      controller_->GetWebContents()->GetNativeView();
  DCHECK(view_android);
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android)
    return;

  java_object_.Reset(Java_AutofillProgressDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));

  Java_AutofillProgressDialogBridge_showDialog(
      env, java_object_, ConvertUTF16ToJavaString(env, controller_->GetTitle()),
      ConvertUTF16ToJavaString(env, controller_->GetLoadingMessage()),
      ConvertUTF16ToJavaString(env, controller_->GetCancelButtonLabel()),
      ResourceMapper::MapToJavaDrawableId(
          IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER));
}

void AutofillProgressDialogViewAndroid::ShowConfirmation() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillProgressDialogBridge_showConfirmation(
        env, java_object_,
        ConvertUTF16ToJavaString(env, controller_->GetConfirmationMessage()));
  }
}

}  // namespace autofill
