// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/view_configuration.h"

#include "base/android/jni_android.h"
#include "jni/ViewConfiguration_jni.h"

using namespace JNI_ViewConfiguration;
using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;

namespace gfx {

int ViewConfiguration::GetDoubleTapTimeoutInMs() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ViewConfiguration_getDoubleTapTimeout(env);
}

int ViewConfiguration::GetLongPressTimeoutInMs() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ViewConfiguration_getLongPressTimeout(env);
}

int ViewConfiguration::GetTapTimeoutInMs() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ViewConfiguration_getTapTimeout(env);
}

int ViewConfiguration::GetMaximumFlingVelocityInPixelsPerSecond() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> view =
      Java_ViewConfiguration_get(env, GetApplicationContext());
  return Java_ViewConfiguration_getScaledMaximumFlingVelocity(env, view.obj());
}

int ViewConfiguration::GetMinimumFlingVelocityInPixelsPerSecond() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> view =
      Java_ViewConfiguration_get(env, GetApplicationContext());
  return Java_ViewConfiguration_getScaledMinimumFlingVelocity(env, view.obj());
}

int ViewConfiguration::GetTouchSlopInPixels() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> view =
      Java_ViewConfiguration_get(env, GetApplicationContext());
  return Java_ViewConfiguration_getScaledTouchSlop(env, view.obj());
}

bool ViewConfiguration::RegisterViewConfiguration(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace gfx
