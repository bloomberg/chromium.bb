// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/event.h"

#include "base/logging.h"
#include "ui/base/event.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"

namespace views {

MouseEvent::MouseEvent(const ui::NativeEvent& native_event)
    : LocatedEvent(static_cast<const ui::LocatedEvent&>(*native_event)) {
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

TouchEvent::TouchEvent(const ui::NativeEvent& event)
    : LocatedEvent(static_cast<const ui::LocatedEvent&>(*event)),
      touch_id_(static_cast<ui::TouchEvent*>(event)->touch_id()),
      radius_x_(static_cast<ui::TouchEvent*>(event)->radius_x()),
      radius_y_(static_cast<ui::TouchEvent*>(event)->radius_y()),
      rotation_angle_(
          static_cast<ui::TouchEvent*>(event)->rotation_angle()),
      force_(static_cast<ui::TouchEvent*>(event)->force()) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(const ui::NativeEvent& native_event)
    : MouseEvent(native_event),
      offset_(ui::GetMouseWheelOffset(native_event->native_event())) {
}

////////////////////////////////////////////////////////////////////////////////
// ScrollEvent, public:

ScrollEvent::ScrollEvent(const ui::NativeEvent& native_event)
    : MouseEvent(native_event) {
  CHECK(ui::GetScrollOffsets(
      native_event->native_event(), &x_offset_, &y_offset_));
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent, public:

GestureEvent::GestureEvent(const ui::NativeEvent& event)
    : LocatedEvent(static_cast<const ui::LocatedEvent&>(*event)),
      details_(static_cast<ui::GestureEvent*>(event)->details()) {
}

}  // namespace views
