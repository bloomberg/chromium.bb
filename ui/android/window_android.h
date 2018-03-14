// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_WINDOW_ANDROID_H_
#define UI_ANDROID_WINDOW_ANDROID_H_

#include <jni.h>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/android/ui_android_export.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace display {
class DisplayAndroidManager;
}  // namespace display

namespace viz {
class BeginFrameSource;
}  // namespace viz

namespace ui {

extern const float kDefaultMouseWheelTickMultiplier;

class WindowAndroidCompositor;
class WindowAndroidObserver;

// Android implementation of the activity window.
// WindowAndroid is also the root of a ViewAndroid tree.
class UI_ANDROID_EXPORT WindowAndroid : public ViewAndroid {
 public:
  static WindowAndroid* FromJavaWindowAndroid(
      const base::android::JavaParamRef<jobject>& jwindow_android);

  WindowAndroid(JNIEnv* env, jobject obj, int display_id, float scroll_factor);

  ~WindowAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Compositor callback relay.
  void OnCompositingDidCommit();

  void AttachCompositor(WindowAndroidCompositor* compositor);
  void DetachCompositor();

  void AddObserver(WindowAndroidObserver* observer);
  void RemoveObserver(WindowAndroidObserver* observer);

  WindowAndroidCompositor* GetCompositor() { return compositor_; }
  viz::BeginFrameSource* GetBeginFrameSource();

  // Runs the provided callback as soon as the current vsync was handled.
  void AddVSyncCompleteCallback(const base::Closure& callback);

  void SetNeedsAnimate();
  void Animate(base::TimeTicks begin_frame_time);
  void OnVSync(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jlong time_micros,
               jlong period_micros);
  void OnVisibilityChanged(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool visible);
  void OnActivityStopped(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void OnActivityStarted(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void SetVSyncPaused(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      bool paused);

  // Return whether the specified Android permission is granted.
  bool HasPermission(const std::string& permission);
  // Return whether the specified Android permission can be requested by Chrome.
  bool CanRequestPermission(const std::string& permission);

  float mouse_wheel_scroll_factor() const { return mouse_wheel_scroll_factor_; }

  static WindowAndroid* CreateForTesting();

  // Return the window token for this window, if one exists.
  base::android::ScopedJavaLocalRef<jobject> GetWindowToken();

 private:
  class WindowBeginFrameSource;
  friend class DisplayAndroidManager;
  friend class WindowBeginFrameSource;

  void SetNeedsBeginFrames(bool needs_begin_frames);
  void RequestVSyncUpdate();

  // ViewAndroid overrides.
  WindowAndroid* GetWindowAndroid() const override;

  // The ID of the display that this window belongs to.
  int display_id() const { return display_id_; }

  base::android::ScopedJavaGlobalRef<jobject> java_window_;
  const int display_id_;
  WindowAndroidCompositor* compositor_;

  base::ObserverList<WindowAndroidObserver> observer_list_;

  std::unique_ptr<WindowBeginFrameSource> begin_frame_source_;
  bool needs_begin_frames_;
  std::list<base::Closure> vsync_complete_callbacks_;
  float mouse_wheel_scroll_factor_;

  DISALLOW_COPY_AND_ASSIGN(WindowAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_WINDOW_ANDROID_H_
