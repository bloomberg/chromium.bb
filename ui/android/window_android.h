// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_WINDOW_ANDROID_H_
#define UI_ANDROID_WINDOW_ANDROID_H_

#include <jni.h>
#include <vector>
#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/vector2d_f.h"

namespace ui {

// Android implementation of the activity window.
class UI_EXPORT WindowAndroid {
 public:
  WindowAndroid(JNIEnv* env, jobject obj);

  void Destroy(JNIEnv* env, jobject obj);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  static bool RegisterWindowAndroid(JNIEnv* env);

  // The content offset is used to translate snapshots to the correct part of
  // the window.
  void set_content_offset(const gfx::Vector2dF& content_offset) {
    content_offset_ = content_offset;
  }

  bool GrabSnapshot(int content_x, int content_y, int width, int height,
                    std::vector<unsigned char>* png_representation);

 private:
  ~WindowAndroid();

  JavaObjectWeakGlobalRef weak_java_window_;
  gfx::Vector2dF content_offset_;

  DISALLOW_COPY_AND_ASSIGN(WindowAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_WINDOW_ANDROID_H_
