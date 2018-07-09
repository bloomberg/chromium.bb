// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_EVENT_FORWARDER_H_
#define UI_ANDROID_EVENT_FORWARDER_H_

#include "base/android/scoped_java_ref.h"

namespace ui {

class ViewAndroid;

class EventForwarder {
 public:
  ~EventForwarder();

  base::android::ScopedJavaLocalRef<jobject> GetJavaWindowAndroid(
      JNIEnv* env,
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

  void OnMouseEvent(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jlong time_ms,
                    jint android_action,
                    jfloat x,
                    jfloat y,
                    jint pointer_id,
                    jfloat pressure,
                    jfloat orientation,
                    jfloat tilt,
                    jint android_changed_button,
                    jint android_button_state,
                    jint android_meta_state,
                    jint tool_type);

  void OnDragEvent(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
                   jint action,
                   jint x,
                   jint y,
                   jint screen_x,
                   jint screen_y,
                   const base::android::JavaParamRef<jobjectArray>& j_mimeTypes,
                   const base::android::JavaParamRef<jstring>& j_content);

  jboolean OnGestureEvent(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj,
                          jint type,
                          jlong time_ms,
                          jfloat scale);

  jboolean OnGenericMotionEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& motion_event,
      jlong time_ms);

  jboolean OnKeyUp(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& key_event,
                   jint key_code);

  jboolean DispatchKeyEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& motion_event);

  void ScrollBy(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& jobj,
                jfloat delta_x,
                jfloat delta_y);

  void ScrollTo(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& jobj,
                jfloat x,
                jfloat y);

  void DoubleTap(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jobj,
                 jlong time_ms,
                 jint x,
                 jint y);

  void StartFling(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jobj,
                  jlong time_ms,
                  jfloat velocity_x,
                  jfloat velocity_y,
                  jboolean synthetic_scroll,
                  jboolean prevent_boosting);

  void CancelFling(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
                   jlong time_ms,
                   jboolean prevent_boosting);

 private:
  friend class ViewAndroid;

  explicit EventForwarder(ViewAndroid* view);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  ViewAndroid* const view_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(EventForwarder);
};

}  // namespace ui

#endif  // UI_ANDROID_EVENT_FORWARDER_H_
