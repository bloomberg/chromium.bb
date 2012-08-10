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
// LocatedEvent, protected:

#if !defined(USE_AURA)
LocatedEvent::LocatedEvent(const ui::NativeEvent& native_event)
    : Event(native_event,
            ui::EventTypeFromNative(native_event),
            ui::EventFlagsFromNative(native_event)),
      location_(ui::EventLocationFromNative(native_event)) {
}
#endif

// TODO(msw): Kill this legacy constructor when we update uses.
LocatedEvent::LocatedEvent(ui::EventType type,
                           const gfx::Point& location,
                           int flags)
    : Event(type, flags),
      location_(location) {
}

LocatedEvent::LocatedEvent(const LocatedEvent& model,
                           View* source,
                           View* target)
    : Event(model),
      location_(model.location_) {
  if (target && target != source)
    View::ConvertPointToView(source, target, &location_);
}

LocatedEvent::LocatedEvent(const LocatedEvent& model, View* root)
    : Event(model),
      location_(model.location_) {
  View::ConvertPointFromWidget(root, &location_);
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(const ui::NativeEvent& native_event)
    : LocatedEvent(native_event) {
}

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
      : LocatedEvent(type, gfx::Point(x, y), flags),
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
// TouchEvent, private:

TouchEvent::TouchEvent(const TouchEvent& model, View* root)
    : LocatedEvent(model, root),
      touch_id_(model.touch_id_),
      radius_x_(model.radius_x_),
      radius_y_(model.radius_y_),
      rotation_angle_(model.rotation_angle_),
      force_(model.force_) {
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

GestureEvent::GestureEvent(const GestureEvent& model, View* root)
    : LocatedEvent(model, root),
      details_(model.details_) {
}

GestureEvent::GestureEvent(ui::EventType type, int x, int y, int flags)
    : LocatedEvent(type, gfx::Point(x, y), flags),
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
