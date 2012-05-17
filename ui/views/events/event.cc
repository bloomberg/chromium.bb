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
// Event, protected:

Event::Event(ui::EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  // Safely initialize the pointer/struct to null/empty.
  memset(&native_event_, 0, sizeof(native_event_));
}

Event::Event(const NativeEvent& native_event, ui::EventType type, int flags)
    : native_event_(native_event),
      type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

#if !defined(USE_AURA)
LocatedEvent::LocatedEvent(const NativeEvent& native_event)
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
// KeyEvent, public:

#if !defined(USE_AURA)
KeyEvent::KeyEvent(const NativeEvent& native_event)
    : Event(native_event,
            ui::EventTypeFromNative(native_event),
            ui::EventFlagsFromNative(native_event)),
      key_code_(ui::KeyboardCodeFromNative(native_event)),
      character_(0),
      unmodified_character_(0) {
}
#endif

KeyEvent::KeyEvent(ui::EventType type,
                   ui::KeyboardCode key_code,
                   int event_flags)
    : Event(type, event_flags),
      key_code_(key_code),
      character_(ui::GetCharacterFromKeyCode(key_code, event_flags)),
      unmodified_character_(0) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(const NativeEvent& native_event)
    : LocatedEvent(native_event) {
}

MouseEvent::MouseEvent(const MouseEvent& model, View* source, View* target)
    : LocatedEvent(model, source, target) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

#if !defined(USE_AURA)
MouseWheelEvent::MouseWheelEvent(const NativeEvent& native_event)
    : MouseEvent(native_event),
      offset_(ui::GetMouseWheelOffset(native_event)) {
}
#endif

MouseWheelEvent::MouseWheelEvent(const ScrollEvent& scroll_event)
    : MouseEvent(scroll_event.native_event()),
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

ui::EventType TouchEvent::GetEventType() const {
  return type();
}

gfx::Point TouchEvent::GetLocation() const {
  return location();
}

int TouchEvent::GetTouchId() const {
  return touch_id_;
}

int TouchEvent::GetEventFlags() const {
  return flags();
}

base::TimeDelta TouchEvent::GetTimestamp() const {
  return base::TimeDelta::FromMilliseconds(time_stamp().ToDoubleT() * 1000);
}

TouchEvent* TouchEvent::Copy() const {
  return new TouchEvent(type(), location().x(), location().y(),
      flags(), touch_id_, radius_x_, radius_y_, rotation_angle_, force_);
}

float TouchEvent::RadiusX() const {
  return radius_x_;
}

float TouchEvent::RadiusY() const {
  return radius_y_;
}

float TouchEvent::RotationAngle() const {
  return rotation_angle_;
}

float TouchEvent::Force() const {
  return force_;
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
      delta_x_(model.delta_x_),
      delta_y_(model.delta_y_) {
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent, private:

GestureEvent::GestureEvent(const GestureEvent& model, View* root)
    : LocatedEvent(model, root),
      delta_x_(model.delta_x_),
      delta_y_(model.delta_y_) {
}

GestureEvent::GestureEvent(ui::EventType type, int x, int y, int flags)
    : LocatedEvent(type, gfx::Point(x, y), flags),
      delta_x_(0),
      delta_y_(0) {
}

GestureEvent::~GestureEvent() {
}

#if !defined(USE_AURA)
int GestureEvent::GetLowestTouchId() const {
  // TODO:
  return 0;
}
#endif

GestureEventForTest::GestureEventForTest(ui::EventType type,
                                         int x,
                                         int y,
                                         int flags)
    : GestureEvent(type, x, y, flags) {
}

}  // namespace views
