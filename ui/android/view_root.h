// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ROOT_H_
#define UI_ANDROID_VIEW_ROOT_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "ui/android/ui_android_export.h"
#include "ui/android/view_android.h"

namespace ui {

// ViewRoot is the root of a ViewAndroid tree.
// TODO(jinsukkim): Hook this up to UI event forwarding flow and
//     substitute WindowAndroid. See https://crbug.com/671401
class UI_ANDROID_EXPORT ViewRoot : public ViewAndroid {
 public:
  explicit ViewRoot(jlong window_android_ptr);

  // ViewAndroid overrides.
  WindowAndroid* GetWindowAndroid() const override;
  ViewAndroid* GetViewRoot() override;

  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj);

  jboolean OnTouchEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& motion_event,
      jlong time_ms,
      jint android_action,
      jint pointer_count,
      jint history_size,
      jint action_index,
      jfloat pos_x_0,
      jfloat pos_y_0,
      jfloat pos_x_1,
      jfloat pos_y_1,
      jint pointer_id_0,
      jint pointer_id_1,
      jfloat touch_major_0,
      jfloat touch_major_1,
      jfloat touch_minor_0,
      jfloat touch_minor_1,
      jfloat orientation_0,
      jfloat orientation_1,
      jfloat tilt_0,
      jfloat tilt_1,
      jfloat raw_pos_x,
      jfloat raw_pos_y,
      jint android_tool_type_0,
      jint android_tool_type_1,
      jint android_button_state,
      jint android_meta_state,
      jboolean is_touch_handle_event);

 private:
  WindowAndroid* window_;

  DISALLOW_COPY_AND_ASSIGN(ViewRoot);
};

bool RegisterViewRoot(JNIEnv* env);

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ROOT_H_
