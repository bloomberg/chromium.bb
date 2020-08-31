// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/android/touch_to_fill_view_impl.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/touch_to_fill/android/jni_headers/Credential_jni.h"
#include "chrome/browser/touch_to_fill/android/jni_headers/TouchToFillBridge_jni.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using password_manager::UiCredential;

namespace {

UiCredential ConvertJavaCredential(JNIEnv* env,
                                   const JavaParamRef<jobject>& credential) {
  return UiCredential(
      ConvertJavaStringToUTF16(env,
                               Java_Credential_getUsername(env, credential)),
      ConvertJavaStringToUTF16(env,
                               Java_Credential_getPassword(env, credential)),
      url::Origin::Create(GURL(ConvertJavaStringToUTF8(
          env, Java_Credential_getOriginUrl(env, credential)))),
      UiCredential::IsPublicSuffixMatch(
          Java_Credential_isPublicSuffixMatch(env, credential)),
      UiCredential::IsAffiliationBasedMatch(
          Java_Credential_isAffiliationBasedMatch(env, credential)));
}

}  // namespace

TouchToFillViewImpl::TouchToFillViewImpl(TouchToFillController* controller)
    : controller_(controller) {}

TouchToFillViewImpl::~TouchToFillViewImpl() {
  if (java_object_internal_) {
    // Don't create an object just for destruction.
    Java_TouchToFillBridge_destroy(AttachCurrentThread(),
                                   java_object_internal_);
  }
}

void TouchToFillViewImpl::Show(
    const GURL& url,
    IsOriginSecure is_origin_secure,
    base::span<const password_manager::UiCredential> credentials) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  // Serialize the |credentials| span into a Java array and instruct the bridge
  // to show it together with |url| to the user.
  JNIEnv* env = AttachCurrentThread();
  auto credential_array =
      Java_TouchToFillBridge_createCredentialArray(env, credentials.size());
  for (size_t i = 0; i < credentials.size(); ++i) {
    const auto& credential = credentials[i];
    Java_TouchToFillBridge_insertCredential(
        env, credential_array, i,
        ConvertUTF16ToJavaString(env, credential.username()),
        ConvertUTF16ToJavaString(env, credential.password()),
        ConvertUTF16ToJavaString(env, GetDisplayUsername(credential)),
        ConvertUTF8ToJavaString(env, credential.origin().Serialize()),
        credential.is_public_suffix_match().value(),
        credential.is_affiliation_based_match().value());
  }

  Java_TouchToFillBridge_showCredentials(
      env, java_object, ConvertUTF8ToJavaString(env, url.spec()),
      is_origin_secure.value(), credential_array);
}

void TouchToFillViewImpl::OnCredentialSelected(const UiCredential& credential) {
  controller_->OnCredentialSelected(credential);
}

void TouchToFillViewImpl::OnDismiss() {
  controller_->OnDismiss();
}

void TouchToFillViewImpl::OnCredentialSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& credential) {
  OnCredentialSelected(ConvertJavaCredential(env, credential));
}

void TouchToFillViewImpl::OnManagePasswordsSelected(JNIEnv* env) {
  controller_->OnManagePasswordsSelected();
}

void TouchToFillViewImpl::OnDismiss(JNIEnv* env) {
  OnDismiss();
}

base::android::ScopedJavaGlobalRef<jobject>
TouchToFillViewImpl::GetOrCreateJavaObject() {
  if (java_object_internal_) {
    return java_object_internal_;
  }
  if (controller_->GetNativeView() == nullptr ||
      controller_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return nullptr;  // No window attached (yet or anymore).
  }
  return java_object_internal_ = Java_TouchToFillBridge_create(
             AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
             controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject());
}
