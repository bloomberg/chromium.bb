// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/android/platform_ime_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/PlatformImeControllerAndroid_jni.h"

namespace ui {

// static
bool PlatformImeControllerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

PlatformImeControllerAndroid::PlatformImeControllerAndroid() {
}

PlatformImeControllerAndroid::~PlatformImeControllerAndroid() {
}

void PlatformImeControllerAndroid::Init(JNIEnv* env, jobject jobj) {
  DCHECK(java_platform_ime_controller_android_.is_empty());
  java_platform_ime_controller_android_ = JavaObjectWeakGlobalRef(env, jobj);
}

void PlatformImeControllerAndroid::UpdateTextInputState(
    const TextInputState& state) {
  if (java_platform_ime_controller_android_.is_empty())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PlatformImeControllerAndroid_updateTextInputState(
      env,
      java_platform_ime_controller_android_.get(env).obj(),
      state.type,
      state.flags,
      base::android::ConvertUTF8ToJavaString(env, state.text).obj(),
      state.selection_start,
      state.selection_end,
      state.composition_start,
      state.composition_end);
}

void PlatformImeControllerAndroid::SetImeVisibility(bool visible) {
  if (java_platform_ime_controller_android_.is_empty())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PlatformImeControllerAndroid_setImeVisibility(
      env,
      java_platform_ime_controller_android_.get(env).obj(),
      visible);
}

}  // namespace ui
