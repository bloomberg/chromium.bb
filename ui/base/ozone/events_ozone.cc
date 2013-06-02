// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/events/event_utils.h"

namespace ui {

base::TimeDelta EventTimeFromNative(const base::NativeEvent& native_event) {
  ui::Event* event = static_cast<ui::Event*>(native_event);
  return event->time_stamp();
}

int EventFlagsFromNative(const base::NativeEvent& native_event) {
  ui::Event* event = static_cast<ui::Event*>(native_event);
  return event->flags();
}

EventType EventTypeFromNative(const base::NativeEvent& native_event) {
  ui::Event* event = static_cast<ui::Event*>(native_event);
  return event->type();
}

gfx::Point EventLocationFromNative(const base::NativeEvent& native_event) {
  ui::LocatedEvent* event = static_cast<ui::LocatedEvent*>(native_event);
  DCHECK(event->IsMouseEvent() || event->IsTouchEvent() ||
         event->IsGestureEvent() || event->IsScrollEvent());
  return event->location();
}

int GetChangedMouseButtonFlagsFromNative(
    const base::NativeEvent& native_event) {
  ui::MouseEvent* event = static_cast<ui::MouseEvent*>(native_event);
  DCHECK(event->IsMouseEvent());
  return event->changed_button_flags();
}

KeyboardCode KeyboardCodeFromNative(const base::NativeEvent& native_event) {
  ui::KeyEvent* event = static_cast<ui::KeyEvent*>(native_event);
  DCHECK(event->IsKeyEvent());
  return event->key_code();
}

gfx::Vector2d GetMouseWheelOffset(const base::NativeEvent& native_event) {
  ui::MouseWheelEvent* event = static_cast<ui::MouseWheelEvent*>(native_event);
  DCHECK(event->type() == ET_MOUSEWHEEL);
  return event->offset();
}

int GetTouchId(const base::NativeEvent& native_event) {
  ui::TouchEvent* event = static_cast<ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->touch_id();
}

float GetTouchRadiusX(const base::NativeEvent& native_event) {
  ui::TouchEvent* event = static_cast<ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->radius_x();
}

float GetTouchRadiusY(const base::NativeEvent& native_event) {
  ui::TouchEvent* event = static_cast<ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->radius_y();
}

float GetTouchAngle(const base::NativeEvent& native_event) {
  ui::TouchEvent* event = static_cast<ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->rotation_angle();
}

float GetTouchForce(const base::NativeEvent& native_event) {
  ui::TouchEvent* event = static_cast<ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->force();
}

bool GetScrollOffsets(const base::NativeEvent& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count) {
  NOTIMPLEMENTED();
  return false;
}

bool GetFlingData(const base::NativeEvent& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ui
