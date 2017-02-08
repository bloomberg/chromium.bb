// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_CLIENT_H_
#define UI_ANDROID_VIEW_CLIENT_H_

#include <jni.h>

#include "ui/android/ui_android_export.h"

namespace ui {

// Container of motion event data. Used when traversing views along their
// hierarchy. Actual motion event object will be constructed right before
// it is used in the |ViewClient| implementation to avoid creating multiple
// |MotionEventAndroid| instances.
struct MotionEventData {
  MotionEventData(float dip_scale,
                  jobject jevent,
                  long time,
                  int action,
                  int pointer_count,
                  int history_size,
                  int action_index,
                  float pos_x0,
                  float pos_y0,
                  float pos_x1,
                  float pos_y1,
                  int pointer_id_0,
                  int pointer_id_1,
                  float touch_major_0,
                  float touch_major_1,
                  float touch_minor_0,
                  float touch_minor_1,
                  float orientation_0,
                  float orientation_1,
                  float tilt_0,
                  float tilt_1,
                  float raw_pos_x,
                  float raw_pos_y,
                  int tool_type_0,
                  int tool_type_1,
                  int button_state,
                  int meta_state,
                  bool is_touch_handle_event);

  MotionEventData(const MotionEventData& other);

  // Returns a new |MotionEventData| object whose position is offset
  // by a given delta.
  MotionEventData Offset(float delta_x, float delta_y) const;

  float GetX() const { return pos_x0_ / dip_scale_; }
  float GetY() const { return pos_y0_ / dip_scale_; }

  const float dip_scale_;
  const jobject jevent_;
  const long time_; // ms
  const int action_;
  const int pointer_count_;
  const int history_size_;
  const int action_index_;

  const float pos_x0_;  // in pixel unit
  const float pos_y0_;
  const float pos_x1_;
  const float pos_y1_;

  const int pointer_id_0_;
  const int pointer_id_1_;
  const float touch_major_0_;
  const float touch_major_1_;
  const float touch_minor_0_;
  const float touch_minor_1_;
  const float orientation_0_;
  const float orientation_1_;
  const float tilt_0_;
  const float tilt_1_;
  const float raw_pos_x_;
  const float raw_pos_y_;
  const int tool_type_0_;
  const int tool_type_1_;
  const int button_state_;
  const int meta_state_;
  const bool is_touch_handle_event_;
};

// Client interface used to forward events from Java to native views.
// Calls are dispatched to its children along the hierarchy of ViewAndroid.
// Use bool return type to stop propagating the call i.e. overriden method
// should return true to indicate that the event was handled and stop
// the processing.
// Note: Not in use yet. Will be hooked up together with ViewRoot.
//       See https://crbug.com/671401.
class UI_ANDROID_EXPORT ViewClient {
 public:
  virtual bool OnTouchEvent(const MotionEventData& event);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_CLIENT_H_
