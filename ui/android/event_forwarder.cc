// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/event_forwarder.h"

#include "base/android/jni_array.h"
#include "base/metrics/histogram_macros.h"
#include "jni/EventForwarder_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/base_event_utils.h"

namespace ui {

using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

EventForwarder::EventForwarder(ViewAndroid* view) : view_(view) {}

EventForwarder::~EventForwarder() {
  if (!java_obj_.is_null()) {
    Java_EventForwarder_destroy(base::android::AttachCurrentThread(),
                                java_obj_);
    java_obj_.Reset();
  }
}

ScopedJavaLocalRef<jobject> EventForwarder::GetJavaObject() {
  if (java_obj_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_obj_.Reset(
        Java_EventForwarder_create(env, reinterpret_cast<intptr_t>(this),
                                   switches::IsTouchDragDropEnabled()));
  }
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

ScopedJavaLocalRef<jobject> EventForwarder::GetJavaWindowAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return view_->GetWindowAndroid()->GetJavaObject();
}

jboolean EventForwarder::OnTouchEvent(JNIEnv* env,
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
                                      jboolean for_touch_handle) {
  ui::MotionEventAndroid::Pointer pointer0(
      pointer_id_0, pos_x_0, pos_y_0, touch_major_0, touch_minor_0,
      orientation_0, tilt_0, android_tool_type_0);
  ui::MotionEventAndroid::Pointer pointer1(
      pointer_id_1, pos_x_1, pos_y_1, touch_major_1, touch_minor_1,
      orientation_1, tilt_1, android_tool_type_1);
  ui::MotionEventAndroid event(
      env, motion_event.obj(), 1.f / view_->GetDipScale(), 0.f, 0.f, 0.f,
      time_ms, android_action, pointer_count, history_size, action_index,
      0 /* action_button */, android_button_state, android_meta_state,
      raw_pos_x - pos_x_0, raw_pos_y - pos_y_0, for_touch_handle, &pointer0,
      &pointer1);
  return view_->OnTouchEvent(event);
}

void EventForwarder::OnMouseEvent(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong time_ms,
                                  jint android_action,
                                  jfloat x,
                                  jfloat y,
                                  jint pointer_id,
                                  jfloat orientation,
                                  jfloat pressure,
                                  jfloat tilt,
                                  jint android_action_button,
                                  jint android_button_state,
                                  jint android_meta_state,
                                  jint android_tool_type) {
  // Construct a motion_event object minimally, only to convert the raw
  // parameters to ui::MotionEvent values. Since we used only the cached values
  // at index=0, it is okay to even pass a null event to the constructor.
  ui::MotionEventAndroid::Pointer pointer(
      pointer_id, x, y, 0.0f /* touch_major */, 0.0f /* touch_minor */,
      orientation, tilt, android_tool_type);
  ui::MotionEventAndroid event(
      env, nullptr /* event */, 1.f / view_->GetDipScale(), 0.f, 0.f, 0.f,
      time_ms, android_action, 1 /* pointer_count */, 0 /* history_size */,
      0 /* action_index */, android_action_button, android_button_state,
      android_meta_state, 0 /* raw_offset_x_pixels */,
      0 /* raw_offset_y_pixels */, false /* for_touch_handle */, &pointer,
      nullptr);
  view_->OnMouseEvent(event);
}

void EventForwarder::OnMouseWheelEvent(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jlong time_ms,
                                       jfloat x,
                                       jfloat y,
                                       jfloat ticks_x,
                                       jfloat ticks_y,
                                       jfloat pixels_per_tick) {
  if (!ticks_x && !ticks_y)
    return;

  // Compute Event.Latency.OS.MOUSE_WHEEL histogram.
  base::TimeTicks current_time = ui::EventTimeForNow();
  base::TimeTicks event_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time_ms);
  base::TimeDelta delta = current_time - event_time;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.MOUSE_WHEEL",
                              delta.InMicroseconds(), 1, 1000000, 50);
  ui::MotionEventAndroid::Pointer pointer(
      0, x, y, 0.0f /* touch_major */, 0.0f /* touch_minor */, 0.0f, 0.0f, 0);
  ui::MotionEventAndroid event(
      env, nullptr, 1.f / view_->GetDipScale(), ticks_x, ticks_y,
      pixels_per_tick, time_ms, 0 /* action */, 1 /* pointer_count */,
      0 /* history_size */, 0 /* action_index */, 0, 0, 0,
      0 /* raw_offset_x_pixels */, 0 /* raw_offset_y_pixels */,
      false /* for_touch_handle */, &pointer, nullptr);

  view_->OnMouseWheelEvent(event);
}

void EventForwarder::OnDragEvent(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj,
                                 jint action,
                                 jint x,
                                 jint y,
                                 jint screen_x,
                                 jint screen_y,
                                 const JavaParamRef<jobjectArray>& j_mimeTypes,
                                 const JavaParamRef<jstring>& j_content) {
  float dip_scale = view_->GetDipScale();
  gfx::PointF location(x / dip_scale, y / dip_scale);
  gfx::PointF root_location(screen_x / dip_scale, screen_y / dip_scale);
  std::vector<base::string16> mime_types;
  AppendJavaStringArrayToStringVector(env, j_mimeTypes, &mime_types);

  DragEventAndroid event(env, action, location, root_location, mime_types,
                         j_content.obj());
  view_->OnDragEvent(event);
}

bool EventForwarder::OnGestureEvent(JNIEnv* env,
                                    const JavaParamRef<jobject>& jobj,
                                    jint type,
                                    jlong time_ms,
                                    jfloat delta) {
  float dip_scale = view_->GetDipScale();
  auto size = view_->GetSize();
  float x = size.width() / 2;
  float y = size.height() / 2;
  gfx::PointF root_location =
      ScalePoint(view_->GetLocationOnScreen(x, y), 1.f / dip_scale);
  return view_->OnGestureEvent(
      GestureEventAndroid(type, gfx::PointF(x / dip_scale, y / dip_scale),
                          root_location, time_ms, delta));
}

}  // namespace ui
