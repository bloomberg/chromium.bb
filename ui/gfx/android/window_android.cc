// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "jni/NativeWindow_jni.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

WindowAndroid::WindowAndroid(JNIEnv* env, jobject obj)
  : weak_java_window_(env, obj) {
}

void WindowAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> WindowAndroid::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  return weak_java_window_.get(env);
}

bool WindowAndroid::RegisterWindowAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WindowAndroid::~WindowAndroid() {
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jint Init(JNIEnv* env, jobject obj) {
  WindowAndroid* window = new WindowAndroid(env, obj);
  return reinterpret_cast<jint>(window);
}

}  // namespace ui
