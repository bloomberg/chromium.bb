// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <sys/system_properties.h>

#include "sync/util/session_utils_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;
using base::android::GetClass;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

ScopedJavaLocalRef<jstring> GetAndroidIdJNI(
    JNIEnv* env, const JavaRef<jobject>& content_resolver) {
  ScopedJavaLocalRef<jclass> clazz(
      GetClass(env, "android/provider/Settings$Secure"));
  jmethodID j_get_string = GetStaticMethodID(env, clazz, "getString",
      "(Landroid/content/ContentResolver;Ljava/lang/String;)"
          "Ljava/lang/String;");
  ScopedJavaLocalRef<jstring> j_android_id =
      ConvertUTF8ToJavaString(env, "android_id");
  jstring android_id = static_cast<jstring>(
      env->CallStaticObjectMethod(
          clazz.obj(), j_get_string, content_resolver.obj(),
          j_android_id.obj()));
  CheckException(env);
  return ScopedJavaLocalRef<jstring>(env, android_id);
}

ScopedJavaLocalRef<jobject> GetContentResolver(JNIEnv* env) {
  ScopedJavaLocalRef<jclass> clazz(GetClass(env, "android/content/Context"));
  jmethodID j_get_content_resolver_method = GetMethodID(
      env, clazz,"getContentResolver", "()Landroid/content/ContentResolver;");
  jobject content_resolver = env->CallObjectMethod(
      GetApplicationContext(), j_get_content_resolver_method);
  CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, content_resolver);
}

}

namespace syncer {
namespace internal {

std::string GetAndroidId() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> android_id =
      GetAndroidIdJNI(env, GetContentResolver(env));
  return ConvertJavaStringToUTF8(android_id);
}

std::string GetModel() {
  char model[PROP_VALUE_MAX];
  __system_property_get("ro.product.model", model);
  return model;
}

bool IsTabletUi() {
  NOTIMPLEMENTED() << "TODO(yfriedman): Upstream this";
  return true;
}

}  // namespace internal
}  // namespace syncer
