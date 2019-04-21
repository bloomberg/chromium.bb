// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/location_bar_model_android.h"

#include "base/android/jni_string.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "jni/LocationBarModel_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

LocationBarModelAndroid::LocationBarModelAndroid(JNIEnv* env,
                                                 const JavaRef<jobject>& obj)
    : location_bar_model_(
          std::make_unique<LocationBarModelImpl>(this,
                                                 content::kMaxURLDisplayChars)),
      java_object_(obj) {}

LocationBarModelAndroid::~LocationBarModelAndroid() {}

void LocationBarModelAndroid::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jstring> LocationBarModelAndroid::GetFormattedFullURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF16ToJavaString(
      env, location_bar_model_->GetFormattedFullURL());
}

ScopedJavaLocalRef<jstring> LocationBarModelAndroid::GetURLForDisplay(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF16ToJavaString(
      env, location_bar_model_->GetURLForDisplay());
}

ScopedJavaLocalRef<jstring> LocationBarModelAndroid::GetDisplaySearchTerms(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  base::string16 result;
  if (!location_bar_model_->GetDisplaySearchTerms(&result))
    return nullptr;

  return base::android::ConvertUTF16ToJavaString(env, result);
}

content::WebContents* LocationBarModelAndroid::GetActiveWebContents() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jweb_contents =
      Java_LocationBarModel_getActiveWebContents(env, java_object_);
  return content::WebContents::FromJavaWebContents(jweb_contents);
}

// static
jlong JNI_LocationBarModel_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new LocationBarModelAndroid(env, obj));
}
