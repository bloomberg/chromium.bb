// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/android_ui_constants.h"

#include "base/android/jni_android.h"
#include "jni/UiConstants_jni.h"

bool AndroidUiConstants::IsFocusRingOutset() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_UiConstants_isFocusRingOutset(env);
}

base::Optional<float> AndroidUiConstants::GetMinimumStrokeWidthForFocusRing() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_UiConstants_hasCustomMinimumStrokeWidthForFocusRing(env))
    return base::nullopt;
  return Java_UiConstants_getMinimumStrokeWidthForFocusRing(env);
}

base::Optional<SkColor> AndroidUiConstants::GetFocusRingColor() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_UiConstants_hasCustomFocusRingColor(env))
    return base::nullopt;
  return Java_UiConstants_getFocusRingColor(env);
}
