// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/event.h"

#include "base/logging.h"
#include "ui/base/event.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(const NativeEvent& native_event)
    : Event(native_event, native_event->type(), native_event->flags()),
      location_(static_cast<ui::LocatedEvent*>(native_event)->location()) {
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

TouchEvent::TouchEvent(const NativeEvent& event)
    : LocatedEvent(event),
      touch_id_(static_cast<ui::TouchEvent*>(event)->touch_id()),
      radius_x_(static_cast<ui::TouchEvent*>(event)->radius_x()),
      radius_y_(static_cast<ui::TouchEvent*>(event)->radius_y()),
      rotation_angle_(
          static_cast<ui::TouchEvent*>(event)->rotation_angle()),
      force_(static_cast<ui::TouchEvent*>(event)->force()) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(const NativeEvent& native_event)
    : MouseEvent(native_event),
      offset_(ui::GetMouseWheelOffset(native_event->native_event())) {
}

////////////////////////////////////////////////////////////////////////////////
// ScrollEvent, public:

ScrollEvent::ScrollEvent(const NativeEvent& native_event)
    : MouseEvent(native_event) {
  CHECK(ui::GetScrollOffsets(
      native_event->native_event(), &x_offset_, &y_offset_));
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent, public:

GestureEvent::GestureEvent(const NativeEvent& event)
    : LocatedEvent(event),
      details_(static_cast<ui::GestureEventImpl*>(event)->details()) {
}

}  // namespace views
