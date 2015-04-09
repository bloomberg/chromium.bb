// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

ViewAndroid::ViewAndroid(jobject view_android_delegate, WindowAndroid* window)
    : window_android_(window) {
  DCHECK(view_android_delegate);
  view_android_delegate_.Reset(AttachCurrentThread(), view_android_delegate);
}

ViewAndroid::~ViewAndroid() {
}

ScopedJavaLocalRef<jobject> ViewAndroid::GetViewAndroidDelegate() {
  return base::android::ScopedJavaLocalRef<jobject>(view_android_delegate_);
}

WindowAndroid* ViewAndroid::GetWindowAndroid() {
  return window_android_;
}

}  // namespace ui
