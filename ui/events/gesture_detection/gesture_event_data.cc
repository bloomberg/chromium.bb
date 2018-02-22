// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_event_data.h"

#include "base/logging.h"

namespace ui {

namespace {

EventPointerType ToEventPointerType(MotionEvent::ToolType tool_type) {
  switch (tool_type) {
    case MotionEvent::ToolType::UNKNOWN:
      return EventPointerType::POINTER_TYPE_UNKNOWN;
    case MotionEvent::ToolType::FINGER:
      return EventPointerType::POINTER_TYPE_TOUCH;
    case MotionEvent::ToolType::STYLUS:
      return EventPointerType::POINTER_TYPE_PEN;
    case MotionEvent::ToolType::MOUSE:
      return EventPointerType::POINTER_TYPE_MOUSE;
    case MotionEvent::ToolType::ERASER:
      return EventPointerType::POINTER_TYPE_ERASER;
    default:
      NOTREACHED() << "Invalid ToolType = " << tool_type;
      return EventPointerType::POINTER_TYPE_UNKNOWN;
  }
}

}  // anonymous namespace

GestureEventData::GestureEventData(const GestureEventDetails& details,
                                   int motion_event_id,
                                   MotionEvent::ToolType primary_tool_type,
                                   base::TimeTicks time,
                                   float x,
                                   float y,
                                   float raw_x,
                                   float raw_y,
                                   size_t touch_point_count,
                                   const gfx::RectF& bounding_box,
                                   int flags,
                                   uint32_t unique_touch_event_id)
    : details(details),
      motion_event_id(motion_event_id),
      primary_tool_type(primary_tool_type),
      time(time),
      x(x),
      y(y),
      raw_x(raw_x),
      raw_y(raw_y),
      flags(flags),
      unique_touch_event_id(unique_touch_event_id) {
  DCHECK_GE(motion_event_id, 0);
  DCHECK_NE(0U, touch_point_count);
  this->details.set_primary_pointer_type(ToEventPointerType(primary_tool_type));
  this->details.set_touch_points(static_cast<int>(touch_point_count));
  this->details.set_bounding_box(bounding_box);
}

GestureEventData::GestureEventData(EventType type,
                                   const GestureEventData& other)
    : details(type, other.details),
      motion_event_id(other.motion_event_id),
      primary_tool_type(other.primary_tool_type),
      time(other.time),
      x(other.x),
      y(other.y),
      raw_x(other.raw_x),
      raw_y(other.raw_y),
      flags(other.flags),
      unique_touch_event_id(other.unique_touch_event_id) {
  details.set_primary_pointer_type(other.details.primary_pointer_type());
  details.set_touch_points(other.details.touch_points());
  details.set_bounding_box(other.details.bounding_box_f());
}

GestureEventData::GestureEventData(const GestureEventData& other) = default;

GestureEventData::GestureEventData()
    : motion_event_id(0),
      primary_tool_type(MotionEvent::ToolType::UNKNOWN),
      x(0),
      y(0),
      raw_x(0),
      raw_y(0),
      flags(EF_NONE),
      unique_touch_event_id(0U) {}

}  //  namespace ui
