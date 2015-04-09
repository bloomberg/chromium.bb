// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_H_
#define UI_ANDROID_VIEW_ANDROID_H_

#include <jni.h>
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "ui/android/ui_android_export.h"

namespace ui {

class WindowAndroid;

// This class is merely a holder of the Java object. It must be destroyed
// explicitly.
class UI_ANDROID_EXPORT ViewAndroid {
 public:
  ViewAndroid(jobject obj, WindowAndroid* window);
  ~ViewAndroid();

  WindowAndroid* GetWindowAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetViewAndroidDelegate();

 private:
  base::android::ScopedJavaGlobalRef<jobject> view_android_delegate_;
  WindowAndroid* window_android_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
