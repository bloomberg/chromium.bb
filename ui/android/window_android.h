// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_WINDOW_ANDROID_H_
#define UI_ANDROID_WINDOW_ANDROID_H_

#include <jni.h>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

class WindowAndroidCompositor;
class WindowAndroidObserver;

// Android implementation of the activity window.
class UI_ANDROID_EXPORT WindowAndroid {
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

  gfx::Vector2dF content_offset() const {
    return content_offset_;
  }

  // Compositor callback relay.
  void OnCompositingDidCommit();

  void AttachCompositor(WindowAndroidCompositor* compositor);
  void DetachCompositor();

  void AddObserver(WindowAndroidObserver* observer);
  void RemoveObserver(WindowAndroidObserver* observer);

  WindowAndroidCompositor* GetCompositor() { return compositor_; }

  void RequestVSyncUpdate();
  void SetNeedsAnimate();
  void OnVSync(JNIEnv* env,
               jobject obj,
               jlong time_micros,
               jlong period_micros);
  void Animate(base::TimeTicks begin_frame_time);

 private:
  ~WindowAndroid();

  JavaObjectWeakGlobalRef weak_java_window_;
  gfx::Vector2dF content_offset_;
  WindowAndroidCompositor* compositor_;

  ObserverList<WindowAndroidObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(WindowAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_WINDOW_ANDROID_H_
