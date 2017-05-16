// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMouseEvent.h"

#include "public/platform/WebGestureEvent.h"

namespace blink {

WebMouseEvent::WebMouseEvent(WebInputEvent::Type type,
                             const WebGestureEvent& gesture_event,
                             Button button_param,
                             int click_count_param,
                             int modifiers,
                             double time_stamp_seconds,
                             PointerId id_param)
    : WebInputEvent(sizeof(WebMouseEvent), type, modifiers, time_stamp_seconds),
      WebPointerProperties(id_param,
                           button_param,
                           WebPointerProperties::PointerType::kMouse),
      click_count(click_count_param),
      position_in_widget_(gesture_event.x, gesture_event.y),
      position_in_screen_(gesture_event.global_x, gesture_event.global_y) {
  SetFrameScale(gesture_event.FrameScale());
  SetFrameTranslate(gesture_event.FrameTranslate());
}

WebFloatPoint WebMouseEvent::MovementInRootFrame() const {
  return WebFloatPoint((movement_x / frame_scale_),
                       (movement_y / frame_scale_));
}

WebFloatPoint WebMouseEvent::PositionInRootFrame() const {
  return WebFloatPoint(
      (position_in_widget_.x / frame_scale_) + frame_translate_.x,
      (position_in_widget_.y / frame_scale_) + frame_translate_.y);
}

WebMouseEvent WebMouseEvent::FlattenTransform() const {
  WebMouseEvent result = *this;
  result.FlattenTransformSelf();
  return result;
}

void WebMouseEvent::FlattenTransformSelf() {
  position_in_widget_.x =
      floor((position_in_widget_.x / frame_scale_) + frame_translate_.x);
  position_in_widget_.y =
      floor((position_in_widget_.y / frame_scale_) + frame_translate_.y);
  frame_translate_.x = 0;
  frame_translate_.y = 0;
  frame_scale_ = 1;
}

}  // namespace blink
