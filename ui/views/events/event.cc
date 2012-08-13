// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/event.h"

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

#if !defined(USE_AURA)
MouseEvent::MouseEvent(const ui::NativeEvent& native_event)
    : LocatedEvent(native_event) {
}
#endif

MouseEvent::MouseEvent(const MouseEvent& model, View* source, View* target)
    : LocatedEvent(model, source, target) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

#if !defined(USE_AURA)
MouseWheelEvent::MouseWheelEvent(const ui::NativeEvent& native_event)
    : MouseEvent(native_event),
      offset_(ui::GetMouseWheelOffset(native_event)) {
}
#endif

MouseWheelEvent::MouseWheelEvent(const ScrollEvent& scroll_event)
    : MouseEvent(scroll_event),
      offset_(scroll_event.y_offset()) {
  set_type(ui::ET_MOUSEWHEEL);
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

TouchEvent::TouchEvent(ui::EventType type,
                       int x,
                       int y,
                       int flags,
                       int touch_id,
                       float radius_x,
                       float radius_y,
                       float angle,
                       float force)
      : LocatedEvent(type, gfx::Point(x, y), gfx::Point(x, y), flags),
        touch_id_(touch_id),
        radius_x_(radius_x),
        radius_y_(radius_y),
        rotation_angle_(angle),
        force_(force) {
}

TouchEvent::TouchEvent(const TouchEvent& model, View* source, View* target)
    : LocatedEvent(model, source, target),
      touch_id_(model.touch_id_),
      radius_x_(model.radius_x_),
      radius_y_(model.radius_y_),
      rotation_angle_(model.rotation_angle_),
      force_(model.force_) {
}

TouchEvent::~TouchEvent() {
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

#if defined(OS_WIN)
// This value matches windows WHEEL_DELTA.
// static
const int MouseWheelEvent::kWheelDelta = 120;
#else
// This value matches GTK+ wheel scroll amount.
const int MouseWheelEvent::kWheelDelta = 53;
#endif

////////////////////////////////////////////////////////////////////////////////
// GestureEvent, public:

GestureEvent::GestureEvent(const GestureEvent& model, View* source,
                           View* target)
    : LocatedEvent(model, source, target),
      details_(model.details_) {
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent, private:

GestureEvent::GestureEvent(ui::EventType type, int x, int y, int flags)
    : LocatedEvent(type, gfx::Point(x, y), gfx::Point(x, y), flags),
      details_(type, 0.f, 0.f) {
}

GestureEvent::~GestureEvent() {
}

GestureEventForTest::GestureEventForTest(ui::EventType type,
                                         int x,
                                         int y,
                                         int flags)
    : GestureEvent(type, x, y, flags) {
}

}  // namespace views
