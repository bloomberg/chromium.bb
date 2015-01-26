// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "ui/events/gestures/motion_event_aura.h"

#include <cmath>

#include "base/logging.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ui {
namespace {

PointerProperties GetPointerPropertiesFromTouchEvent(const TouchEvent& touch) {
  PointerProperties pointer_properties;
  pointer_properties.x = touch.x();
  pointer_properties.y = touch.y();
  pointer_properties.raw_x = touch.root_location_f().x();
  pointer_properties.raw_y = touch.root_location_f().y();
  pointer_properties.id = touch.touch_id();
  pointer_properties.pressure = touch.force();
  pointer_properties.source_device_id = touch.source_device_id();

  float radius_x = touch.radius_x();
  float radius_y = touch.radius_y();
  float rotation_angle_rad = touch.rotation_angle() * M_PI / 180.f;

  DCHECK_GE(radius_x, 0) << "Unexpected x-radius < 0";
  DCHECK_GE(radius_y, 0) << "Unexpected y-radius < 0";
  DCHECK(0 <= rotation_angle_rad && rotation_angle_rad < M_PI)
      << "Unexpected touch rotation angle";

  // Make the angle acute to ease subsequent logic. The angle range effectively
  // changes from [0, pi) to [0, pi/2).
  if (rotation_angle_rad >= M_PI_2) {
    rotation_angle_rad -= static_cast<float>(M_PI_2);
    std::swap(radius_x, radius_y);
  }

  if (radius_x > radius_y) {
    // The case radius_x == radius_y is omitted from here on purpose: for
    // circles, we want to pass the angle (which could be any value in such
    // cases but always seem to be set to zero) unchanged.
    pointer_properties.touch_major = 2.f * radius_x;
    pointer_properties.touch_minor = 2.f * radius_y;
    pointer_properties.orientation = rotation_angle_rad - M_PI_2;
  } else {
    pointer_properties.touch_major = 2.f * radius_y;
    pointer_properties.touch_minor = 2.f * radius_x;
    pointer_properties.orientation = rotation_angle_rad;
  }

  if (!pointer_properties.touch_major) {
    pointer_properties.touch_major =
        2.f * GestureConfiguration::GetInstance()->default_radius();
    pointer_properties.touch_minor =
        2.f * GestureConfiguration::GetInstance()->default_radius();
    pointer_properties.orientation = 0;
  }

  // TODO(jdduke): Plumb tool type from the platform, crbug.com/404128.
  pointer_properties.tool_type = MotionEvent::TOOL_TYPE_UNKNOWN;

  return pointer_properties;
}

}  // namespace

MotionEventAura::MotionEventAura() {
  set_action_index(-1);
}

MotionEventAura::~MotionEventAura() {
}

bool MotionEventAura::OnTouch(const TouchEvent& touch) {
  int index = FindPointerIndexOfId(touch.touch_id());
  bool pointer_id_is_active = index != -1;

  if (touch.type() == ET_TOUCH_PRESSED && pointer_id_is_active) {
    // TODO(tdresser): This should return false (or NOTREACHED()), and
    // ignore the touch; however, there is at least one case where we
    // need to allow a touch press from a currently used touch id. See
    // crbug.com/446852 for details.

    // Cancel the existing touch, before handling the touch press.
    TouchEvent cancel(ET_TOUCH_CANCELLED, touch.location(), touch.touch_id(),
                      touch.time_stamp());
    OnTouch(cancel);
    CleanupRemovedTouchPoints(cancel);
    DCHECK_EQ(-1, FindPointerIndexOfId(touch.touch_id()));
  } else if (touch.type() != ET_TOUCH_PRESSED && !pointer_id_is_active) {
    // We could have an active touch stream transfered to us, resulting in touch
    // move or touch up events without associated touch down events. Ignore
    // them.
    return false;
  }

  if (touch.type() == ET_TOUCH_MOVED && touch.x() == GetX(index) &&
      touch.y() == GetY(index)) {
    return false;
  }

  switch (touch.type()) {
    case ET_TOUCH_PRESSED:
      AddTouch(touch);
      break;
    case ET_TOUCH_RELEASED:
    case ET_TOUCH_CANCELLED:
      // Removing these touch points needs to be postponed until after the
      // MotionEvent has been dispatched. This cleanup occurs in
      // CleanupRemovedTouchPoints.
      UpdateTouch(touch);
      break;
    case ET_TOUCH_MOVED:
      UpdateTouch(touch);
      break;
    default:
      NOTREACHED();
      return false;
  }

  UpdateCachedAction(touch);
  set_flags(touch.flags());
  set_event_time(touch.time_stamp() + base::TimeTicks());
  return true;
}

int MotionEventAura::GetId() const {
  return GetPointerId(0);
}

void MotionEventAura::CleanupRemovedTouchPoints(const TouchEvent& event) {
  if (event.type() != ET_TOUCH_RELEASED &&
      event.type() != ET_TOUCH_CANCELLED) {
    return;
  }

  DCHECK(GetPointerCount());
  int index_to_delete = GetIndexFromId(event.touch_id());
  set_action_index(0);
  pointer(index_to_delete) = pointer(GetPointerCount() - 1);
  PopPointer();
}

int MotionEventAura::GetSourceDeviceId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return pointer(pointer_index).source_device_id;
}

void MotionEventAura::AddTouch(const TouchEvent& touch) {
  if (GetPointerCount() == MotionEvent::MAX_TOUCH_POINT_COUNT)
    return;

  PushPointer(GetPointerPropertiesFromTouchEvent(touch));
}

void MotionEventAura::UpdateTouch(const TouchEvent& touch) {
  pointer(GetIndexFromId(touch.touch_id())) =
      GetPointerPropertiesFromTouchEvent(touch);
}

void MotionEventAura::UpdateCachedAction(const TouchEvent& touch) {
  DCHECK(GetPointerCount());
  switch (touch.type()) {
    case ET_TOUCH_PRESSED:
      if (GetPointerCount() == 1) {
        set_action(ACTION_DOWN);
      } else {
        set_action(ACTION_POINTER_DOWN);
        set_action_index(GetIndexFromId(touch.touch_id()));
      }
      break;
    case ET_TOUCH_RELEASED:
      if (GetPointerCount() == 1) {
        set_action(ACTION_UP);
      } else {
        set_action(ACTION_POINTER_UP);
        set_action_index(GetIndexFromId(touch.touch_id()));
      }
      break;
    case ET_TOUCH_CANCELLED:
      set_action(ACTION_CANCEL);
      break;
    case ET_TOUCH_MOVED:
      set_action(ACTION_MOVE);
      break;
    default:
      NOTREACHED();
      break;
  }
}

int MotionEventAura::GetIndexFromId(int id) const {
  int index = FindPointerIndexOfId(id);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(GetPointerCount()));
  return index;
}

}  // namespace ui
