// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_manager/password_generation_dialog_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string16.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "jni/PasswordGenerationDialogBridge_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

PasswordGenerationDialogViewAndroid::PasswordGenerationDialogViewAndroid(
    PasswordAccessoryController* controller)
    : controller_(controller) {
  ui::WindowAndroid* window_android = controller_->native_window();

  DCHECK(window_android);
  java_object_.Reset(Java_PasswordGenerationDialogBridge_create(
      base::android::AttachCurrentThread(), window_android->GetJavaObject(),
      reinterpret_cast<long>(this)));
}

PasswordGenerationDialogViewAndroid::~PasswordGenerationDialogViewAndroid() {
  DCHECK(!java_object_.is_null());
  Java_PasswordGenerationDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void PasswordGenerationDialogViewAndroid::Show(base::string16& password) {
  JNIEnv* env = base::android::AttachCurrentThread();

   base::string16 explanation_text =
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROMPT);

  Java_PasswordGenerationDialogBridge_showDialog(
      env, java_object_, base::android::ConvertUTF16ToJavaString(env, password),
      base::android::ConvertUTF16ToJavaString(env, explanation_text));
}

void PasswordGenerationDialogViewAndroid::PasswordAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& password) {
  controller_->GeneratedPasswordAccepted(
      base::android::ConvertJavaStringToUTF16(env, password));
}

void PasswordGenerationDialogViewAndroid::PasswordRejected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->GeneratedPasswordRejected();
}

// static
std::unique_ptr<PasswordGenerationDialogViewInterface>
PasswordGenerationDialogViewInterface::Create(
    PasswordAccessoryController* controller) {
  return std::make_unique<PasswordGenerationDialogViewAndroid>(controller);
}
