// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_manager/manual_filling_view_android.h"

#include <jni.h>

#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "components/autofill/core/browser/accessory_sheet_data.h"
#include "jni/ManualFillingBridge_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using autofill::AccessorySheetData;
using autofill::FooterCommand;
using autofill::UserInfo;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

ManualFillingViewAndroid::ManualFillingViewAndroid(
    ManualFillingController* controller)
    : controller_(controller) {
  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);
  java_object_.Reset(Java_ManualFillingBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject()));
}

ManualFillingViewAndroid::~ManualFillingViewAndroid() {
  DCHECK(!java_object_.is_null());
  Java_ManualFillingBridge_destroy(base::android::AttachCurrentThread(),
                                   java_object_);
  java_object_.Reset(nullptr);
}

void ManualFillingViewAndroid::OnItemsAvailable(
    const AccessorySheetData& data) {
  DCHECK(!java_object_.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ManualFillingBridge_onItemsAvailable(
      env, java_object_, ConvertAccessorySheetDataToJavaObject(env, data));
}

void ManualFillingViewAndroid::CloseAccessorySheet() {
  Java_ManualFillingBridge_closeAccessorySheet(
      base::android::AttachCurrentThread(), java_object_);
}

void ManualFillingViewAndroid::SwapSheetWithKeyboard() {
  Java_ManualFillingBridge_swapSheetWithKeyboard(
      base::android::AttachCurrentThread(), java_object_);
}

void ManualFillingViewAndroid::ShowWhenKeyboardIsVisible() {
  Java_ManualFillingBridge_showWhenKeyboardIsVisible(
      base::android::AttachCurrentThread(), java_object_);
}

void ManualFillingViewAndroid::Hide() {
  Java_ManualFillingBridge_hide(base::android::AttachCurrentThread(),
                                java_object_);
}

void ManualFillingViewAndroid::OnAutomaticGenerationStatusChanged(
    bool available) {
  if (!available && java_object_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ManualFillingBridge_onAutomaticGenerationStatusChanged(env, java_object_,
                                                              available);
}

void ManualFillingViewAndroid::OnFaviconRequested(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint desiredSizeInPx,
    const base::android::JavaParamRef<jobject>& j_callback) {
  controller_->GetFavicon(
      desiredSizeInPx,
      base::BindOnce(&ManualFillingViewAndroid::OnImageFetched,
                     base::Unretained(this),  // Outlives or cancels request.
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ManualFillingViewAndroid::OnFillingTriggered(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean isPassword,
    const base::android::JavaParamRef<_jstring*>& textToFill) {
  controller_->OnFillingTriggered(
      isPassword, base::android::ConvertJavaStringToUTF16(textToFill));
}

void ManualFillingViewAndroid::OnOptionSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<_jstring*>& selectedOption) {
  controller_->OnOptionSelected(
      base::android::ConvertJavaStringToUTF16(selectedOption));
}

void ManualFillingViewAndroid::OnGenerationRequested(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnGenerationRequested();
}

void ManualFillingViewAndroid::OnImageFetched(
    const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
    const gfx::Image& image) {
  base::android::ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty())
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());

  RunObjectCallbackAndroid(j_callback, j_bitmap);
}

ScopedJavaLocalRef<jobject>
ManualFillingViewAndroid::ConvertAccessorySheetDataToJavaObject(
    JNIEnv* env,
    const AccessorySheetData& tab_data) {
  ScopedJavaLocalRef<jobject> j_tab_data =
      Java_ManualFillingBridge_createAccessorySheetData(
          env, ConvertUTF16ToJavaString(env, tab_data.title()));

  for (const UserInfo& user_info : tab_data.user_info_list()) {
    ScopedJavaLocalRef<jobject> j_user_info =
        Java_ManualFillingBridge_addUserInfoToAccessorySheetData(env,
                                                                 j_tab_data);
    for (const UserInfo::Field& field : user_info.fields()) {
      Java_ManualFillingBridge_addFieldToUserInfo(
          env, java_object_, j_user_info,
          ConvertUTF16ToJavaString(env, field.display_text()),
          ConvertUTF16ToJavaString(env, field.a11y_description()),
          field.is_obfuscated(), field.selectable());
    }
  }

  for (const FooterCommand& footer_command : tab_data.footer_commands()) {
    Java_ManualFillingBridge_addFooterCommandToAccessorySheetData(
        env, java_object_, j_tab_data,
        ConvertUTF16ToJavaString(env, footer_command.display_text()));
  }
  return j_tab_data;
}

// static
std::unique_ptr<ManualFillingViewInterface> ManualFillingViewInterface::Create(
    ManualFillingController* controller) {
  return std::make_unique<ManualFillingViewAndroid>(controller);
}
