// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event.h"

#include "ui/aura/window.h"
#include "ui/gfx/point3.h"
#include "ui/gfx/transform.h"

namespace aura {

Event::Event(ui::EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  Init();
}

Event::Event(const base::NativeEvent& native_event,
             ui::EventType type,
             int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  InitWithNativeEvent(native_event);
}

Event::Event(const Event& copy)
    : native_event_(copy.native_event_),
      type_(copy.type_),
      time_stamp_(copy.time_stamp_),
      flags_(copy.flags_) {
}

void Event::Init() {
  memset(&native_event_, 0, sizeof(native_event_));
}

void Event::InitWithNativeEvent(const base::NativeEvent& native_event) {
  native_event_ = native_event;
}

LocatedEvent::LocatedEvent(const base::NativeEvent& native_event)
    : Event(native_event,
            ui::EventTypeFromNative(native_event),
            ui::EventFlagsFromNative(native_event)),
      location_(ui::EventLocationFromNative(native_event)) {
}

LocatedEvent::LocatedEvent(const LocatedEvent& model,
                           Window* source,
                           Window* target)
    : Event(model),
      location_(model.location_) {
  if (target && target != source)
    Window::ConvertPointToWindow(source, target, &location_);
}

LocatedEvent::LocatedEvent(ui::EventType type,
                           const gfx::Point& location,
                           int flags)
    : Event(type, flags),
      location_(location) {
}

void LocatedEvent::UpdateForTransform(const ui::Transform& transform) {
  gfx::Point3f p(location_);
  transform.TransformPointReverse(p);
  location_ = p.AsPoint();
}

MouseEvent::MouseEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event) {
}

MouseEvent::MouseEvent(const MouseEvent& model, Window* source, Window* target)
    : LocatedEvent(model, source, target) {
}

MouseEvent::MouseEvent(const MouseEvent& model,
                       Window* source,
                       Window* target,
                       ui::EventType type,
                       int flags)
    : LocatedEvent(model, source, target) {
  set_type(type);
  set_flags(flags);
}

MouseEvent::MouseEvent(ui::EventType type,
                       const gfx::Point& location,
                       int flags)
    : LocatedEvent(type, location, flags) {
}

TouchEvent::TouchEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event),
      touch_id_(ui::GetTouchId(native_event)),
      radius_x_(ui::GetTouchRadiusX(native_event)),
      radius_y_(ui::GetTouchRadiusY(native_event)),
      rotation_angle_(ui::GetTouchAngle(native_event)),
      force_(ui::GetTouchForce(native_event)) {
}

TouchEvent::TouchEvent(const TouchEvent& model,
                       Window* source,
                       Window* target)
    : LocatedEvent(model, source, target),
      touch_id_(model.touch_id_),
      radius_x_(model.radius_x_),
      radius_y_(model.radius_y_),
      rotation_angle_(model.rotation_angle_),
      force_(model.force_) {
}

TouchEvent::TouchEvent(ui::EventType type,
                       const gfx::Point& location,
                       int touch_id)
    : LocatedEvent(type, location, 0),
      touch_id_(touch_id),
      radius_x_(1.0f),
      radius_y_(1.0f),
      rotation_angle_(0.0f),
      force_(0.0f) {
}

KeyEvent::KeyEvent(const base::NativeEvent& native_event, bool is_char)
    : Event(native_event,
            ui::EventTypeFromNative(native_event),
            ui::EventFlagsFromNative(native_event)),
      key_code_(ui::KeyboardCodeFromNative(native_event)),
      is_char_(is_char) {
}

KeyEvent::KeyEvent(ui::EventType type,
                   ui::KeyboardCode key_code,
                   int flags)
    : Event(type, flags),
      key_code_(key_code),
      is_char_(false) {
}

}  // namespace aura
