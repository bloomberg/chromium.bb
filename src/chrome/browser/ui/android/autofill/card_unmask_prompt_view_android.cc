// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/card_unmask_prompt_view_android.h"

#include "chrome/android/chrome_jni_headers/CardUnmaskBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

CardUnmaskPromptView* CreateCardUnmaskPromptView(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents) {
  return new CardUnmaskPromptViewAndroid(controller, web_contents);
}

CardUnmaskPromptViewAndroid::CardUnmaskPromptViewAndroid(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
}

CardUnmaskPromptViewAndroid::~CardUnmaskPromptViewAndroid() {
  if (controller_)
    controller_->OnUnmaskDialogClosed();
}

void CardUnmaskPromptViewAndroid::Show() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  if (view_android == nullptr || view_android->GetWindowAndroid() == nullptr)
    return;

  Java_CardUnmaskBridge_show(env, java_object,
                             view_android->GetWindowAndroid()->GetJavaObject());
}

bool CardUnmaskPromptViewAndroid::CheckUserInputValidity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& response) {
  return controller_->InputCvcIsValid(
      base::android::ConvertJavaStringToUTF16(env, response));
}

void CardUnmaskPromptViewAndroid::OnUserInput(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& cvc,
    const JavaParamRef<jstring>& month,
    const JavaParamRef<jstring>& year,
    jboolean should_store_locally,
    jboolean enable_fido_auth) {
  controller_->OnUnmaskPromptAccepted(
      base::android::ConvertJavaStringToUTF16(env, cvc),
      base::android::ConvertJavaStringToUTF16(env, month),
      base::android::ConvertJavaStringToUTF16(env, year), should_store_locally,
      enable_fido_auth);
}

void CardUnmaskPromptViewAndroid::OnNewCardLinkClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  controller_->NewCardLinkClicked();
  Java_CardUnmaskBridge_update(env, java_object,
                               base::android::ConvertUTF16ToJavaString(
                                   env, controller_->GetWindowTitle()),
                               base::android::ConvertUTF16ToJavaString(
                                   env, controller_->GetInstructionsMessage()),
                               controller_->ShouldRequestExpirationDate());
}

int CardUnmaskPromptViewAndroid::GetExpectedCvcLength(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return controller_->GetExpectedCvcLength();
}

void CardUnmaskPromptViewAndroid::PromptDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  delete this;
}

void CardUnmaskPromptViewAndroid::ControllerGone() {
  controller_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_internal_)
    return;
  Java_CardUnmaskBridge_dismiss(env, java_object_internal_);
}

void CardUnmaskPromptViewAndroid::DisableAndWaitForVerification() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CardUnmaskBridge_disableAndWaitForVerification(env, java_object);
}

void CardUnmaskPromptViewAndroid::GotVerificationResult(
    const base::string16& error_message,
    bool allow_retry) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> message;
  if (!error_message.empty())
      message = base::android::ConvertUTF16ToJavaString(env, error_message);

  Java_CardUnmaskBridge_verificationFinished(env, java_object, message,
                                             allow_retry);
}

base::android::ScopedJavaGlobalRef<jobject>
CardUnmaskPromptViewAndroid::GetOrCreateJavaObject() {
  if (java_object_internal_) {
    return java_object_internal_;
  }
  if (web_contents_->GetNativeView() == nullptr ||
      web_contents_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return nullptr;  // No window attached (yet or anymore).
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  ScopedJavaLocalRef<jstring> dialog_title =
      base::android::ConvertUTF16ToJavaString(env,
                                              controller_->GetWindowTitle());
  ScopedJavaLocalRef<jstring> instructions =
      base::android::ConvertUTF16ToJavaString(
          env, controller_->GetInstructionsMessage());
  ScopedJavaLocalRef<jstring> confirm = base::android::ConvertUTF16ToJavaString(
      env, controller_->GetOkButtonLabel());

  return java_object_internal_ = Java_CardUnmaskBridge_create(
             env, reinterpret_cast<intptr_t>(this), dialog_title, instructions,
             confirm,
             ResourceMapper::MapToJavaDrawableId(controller_->GetCvcImageRid()),
             controller_->ShouldRequestExpirationDate(),
             controller_->CanStoreLocally(),
             controller_->GetStoreLocallyStartState(),
             controller_->ShouldOfferWebauthn(),
             controller_->GetWebauthnOfferStartState(),
             controller_->GetSuccessMessageDuration().InMilliseconds(),
             view_android->GetWindowAndroid()->GetJavaObject());
}

}  // namespace autofill
