// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_root.h"

#include "jni/ViewRoot_jni.h"

namespace ui {

using base::android::JavaParamRef;

ViewRoot::ViewRoot(jlong window_android_ptr) {
  WindowAndroid* window_android =
          reinterpret_cast<WindowAndroid*>(window_android_ptr);
  window_ = window_android;

  // ViewRoot simply accepts all events and lets the hit testing
  // start from its children.
  SetLayout(0, 0, 0, 0, true);
}

ViewAndroid* ViewRoot::GetViewRoot() {
  return this;
}

WindowAndroid* ViewRoot::GetWindowAndroid() const {
  return window_;
}

void ViewRoot::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

jboolean ViewRoot::OnTouchEvent(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jobject>& motion_event,
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
                                jboolean is_touch_handle_event) {
  MotionEventData event(GetDipScale(),
                        motion_event.obj(),
                        time_ms,
                        android_action,
                        pointer_count,
                        history_size,
                        action_index,
                        pos_x_0,
                        pos_y_0,
                        pos_x_1,
                        pos_y_1,
                        pointer_id_0,
                        pointer_id_1,
                        touch_major_0,
                        touch_major_1,
                        touch_minor_0,
                        touch_minor_1,
                        orientation_0,
                        orientation_1,
                        tilt_0,
                        tilt_1,
                        raw_pos_x,
                        raw_pos_y,
                        android_tool_type_0,
                        android_tool_type_1,
                        android_button_state,
                        android_meta_state,
                        is_touch_handle_event);
  return OnTouchEventInternal(event);
}

// static
jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           jlong window_android_ptr) {
  return reinterpret_cast<intptr_t>(new ViewRoot(window_android_ptr));
}

bool RegisterViewRoot(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace ui
