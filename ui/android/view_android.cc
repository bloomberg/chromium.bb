// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_android.h"

#include "base/android/jni_android.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

ViewAndroid::ViewAndroid(jobject obj, WindowAndroid* window)
    : window_android_(window) {
  java_view_.Reset(AttachCurrentThread(), obj);
}

ViewAndroid::~ViewAndroid() {
}

ScopedJavaLocalRef<jobject> ViewAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_view_);
}

WindowAndroid* ViewAndroid::GetWindowAndroid() {
  return window_android_;
}

}  // namespace ui
