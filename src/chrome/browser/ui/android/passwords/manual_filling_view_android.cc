// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/manual_filling_view_android.h"

#include <jni.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_controller_impl.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/browser/password_manager/password_accessory_metrics_util.h"
#include "chrome/browser/password_manager/password_generation_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/common/password_form.h"
#include "jni/ManualFillingComponentBridge_jni.h"
#include "jni/UserInfoField_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using autofill::AccessorySheetData;
using autofill::FooterCommand;
using autofill::UserInfo;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ManualFillingViewAndroid::ManualFillingViewAndroid(
    ManualFillingController* controller)
    : controller_(controller) {
  ui::ViewAndroid* view_android = controller_->container_view();
  DCHECK(view_android);
  java_object_.Reset(Java_ManualFillingComponentBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject()));
}

ManualFillingViewAndroid::~ManualFillingViewAndroid() {
  DCHECK(!java_object_.is_null());
  Java_ManualFillingComponentBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
  java_object_.Reset(nullptr);
}

void ManualFillingViewAndroid::OnItemsAvailable(
    const AccessorySheetData& data) {
  DCHECK(!java_object_.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ManualFillingComponentBridge_onItemsAvailable(
      env, java_object_, ConvertAccessorySheetDataToJavaObject(env, data));
}

void ManualFillingViewAndroid::CloseAccessorySheet() {
  Java_ManualFillingComponentBridge_closeAccessorySheet(
      base::android::AttachCurrentThread(), java_object_);
}

void ManualFillingViewAndroid::SwapSheetWithKeyboard() {
  Java_ManualFillingComponentBridge_swapSheetWithKeyboard(
      base::android::AttachCurrentThread(), java_object_);
}

void ManualFillingViewAndroid::ShowWhenKeyboardIsVisible() {
  Java_ManualFillingComponentBridge_showWhenKeyboardIsVisible(
      base::android::AttachCurrentThread(), java_object_);
}

void ManualFillingViewAndroid::ShowTouchToFillSheet() {
  Java_ManualFillingComponentBridge_showTouchToFillSheet(
      base::android::AttachCurrentThread(), java_object_);
}

void ManualFillingViewAndroid::Hide() {
  Java_ManualFillingComponentBridge_hide(base::android::AttachCurrentThread(),
                                         java_object_);
}

void ManualFillingViewAndroid::OnAutomaticGenerationStatusChanged(
    bool available) {
  if (!available && java_object_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ManualFillingComponentBridge_onAutomaticGenerationStatusChanged(
      env, java_object_, available);
}

void ManualFillingViewAndroid::OnFaviconRequested(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint desired_size_in_px,
    const base::android::JavaParamRef<jobject>& j_callback) {
  controller_->GetFavicon(
      desired_size_in_px,
      base::BindOnce(&ManualFillingViewAndroid::OnImageFetched,
                     base::Unretained(this),  // Outlives or cancels request.
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ManualFillingViewAndroid::OnFillingTriggered(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint tab_type,
    const base::android::JavaParamRef<jobject>& j_user_info_field) {
  controller_->OnFillingTriggered(
      static_cast<autofill::AccessoryTabType>(tab_type),
      ConvertJavaUserInfoField(env, j_user_info_field));
}

void ManualFillingViewAndroid::OnOptionSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint selected_action) {
  controller_->OnOptionSelected(
      static_cast<autofill::AccessoryAction>(selected_action));
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
      Java_ManualFillingComponentBridge_createAccessorySheetData(
          env, static_cast<int>(tab_data.get_sheet_type()),
          ConvertUTF16ToJavaString(env, tab_data.title()));

  for (const UserInfo& user_info : tab_data.user_info_list()) {
    ScopedJavaLocalRef<jobject> j_user_info =
        Java_ManualFillingComponentBridge_addUserInfoToAccessorySheetData(
            env, java_object_, j_tab_data);
    for (const UserInfo::Field& field : user_info.fields()) {
      Java_ManualFillingComponentBridge_addFieldToUserInfo(
          env, java_object_, j_user_info,
          static_cast<int>(tab_data.get_sheet_type()),
          ConvertUTF16ToJavaString(env, field.display_text()),
          ConvertUTF16ToJavaString(env, field.a11y_description()),
          field.is_obfuscated(), field.selectable());
    }
  }

  for (const FooterCommand& footer_command : tab_data.footer_commands()) {
    Java_ManualFillingComponentBridge_addFooterCommandToAccessorySheetData(
        env, java_object_, j_tab_data,
        ConvertUTF16ToJavaString(env, footer_command.display_text()),
        static_cast<int>(footer_command.accessory_action()));
  }
  return j_tab_data;
}

UserInfo::Field ManualFillingViewAndroid::ConvertJavaUserInfoField(
    JNIEnv* env,
    const JavaRef<jobject>& j_field_to_convert) {
  base::string16 display_text = ConvertJavaStringToUTF16(
      env, Java_UserInfoField_getDisplayText(env, j_field_to_convert));
  base::string16 a11y_description = ConvertJavaStringToUTF16(
      env, Java_UserInfoField_getA11yDescription(env, j_field_to_convert));
  bool is_obfuscated = Java_UserInfoField_isObfuscated(env, j_field_to_convert);
  bool selectable = Java_UserInfoField_isSelectable(env, j_field_to_convert);
  return UserInfo::Field(display_text, a11y_description, is_obfuscated,
                         selectable);
}

// static
void JNI_ManualFillingComponentBridge_CachePasswordSheetDataForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jobjectArray>& j_usernames,
    const base::android::JavaParamRef<jobjectArray>& j_passwords) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  PasswordAccessoryController* pwd_controller =
      static_cast<ManualFillingControllerImpl*>(
          ManualFillingControllerImpl::GetOrCreate(web_contents).get())
          ->password_controller_for_testing();

  if (!pwd_controller) {
    // If the controller isn't initialized (e.g. because the flags are not set),
    // fail silently so tests can use shared setup methods for old and new UI.
    LOG(ERROR) << "Tried to fill cache of non-existent accessory controller.";
    return;
  }

  url::Origin origin = url::Origin::Create(web_contents->GetLastCommittedURL());
  std::vector<std::string> usernames;
  std::vector<std::string> passwords;
  base::android::AppendJavaStringArrayToStringVector(env, j_usernames,
                                                     &usernames);
  base::android::AppendJavaStringArrayToStringVector(env, j_passwords,
                                                     &passwords);
  std::vector<autofill::PasswordForm> password_forms(usernames.size());
  std::map<base::string16, const autofill::PasswordForm*> credentials;
  for (unsigned int i = 0; i < usernames.size(); ++i) {
    password_forms[i].origin = origin.GetURL();
    password_forms[i].username_value = base::ASCIIToUTF16(usernames[i]);
    password_forms[i].password_value = base::ASCIIToUTF16(passwords[i]);
    credentials[password_forms[i].username_value] = &password_forms[i];
  }
  pwd_controller->SavePasswordsForOrigin(credentials, origin);
}

// static
void JNI_ManualFillingComponentBridge_SignalAutoGenerationStatusForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jboolean j_available) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (j_available) {
    // The added generation button will call back to the generation controller
    // so we need to make sure one exists.
    ignore_result(PasswordGenerationController::GetOrCreate(web_contents));
  }

  // Bypass the generation controller when sending this status to the UI to
  // avoid setup overhead, since its logic is currently not needed for tests.
  ManualFillingControllerImpl::GetOrCreate(web_contents)
      ->OnAutomaticGenerationStatusChanged(j_available);
}

// static
std::unique_ptr<ManualFillingViewInterface> ManualFillingViewInterface::Create(
    ManualFillingController* controller) {
  return std::make_unique<ManualFillingViewAndroid>(controller);
}
