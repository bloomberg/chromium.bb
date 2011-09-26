// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event.h"

#include "ui/aura/window.h"

namespace aura {

Event::Event(ui::EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  Init();
}

Event::Event(const ui::NativeEvent& native_event, ui::EventType type, int flags)
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

void Event::InitWithNativeEvent(const ui::NativeEvent& native_event) {
  native_event_ = native_event;
}

LocatedEvent::LocatedEvent(const ui::NativeEvent& native_event)
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

MouseEvent::MouseEvent(const ui::NativeEvent& native_event)
    : LocatedEvent(native_event) {
}

MouseEvent::MouseEvent(const MouseEvent& model, Window* source, Window* target)
    : LocatedEvent(model, source, target) {
}

MouseEvent::MouseEvent(const MouseEvent& model,
                       Window* source,
                       Window* target,
                       ui::EventType type)
    : LocatedEvent(model, source, target) {
  set_type(type);
}

MouseEvent::MouseEvent(ui::EventType type,
                       const gfx::Point& location,
                       int flags)
    : LocatedEvent(type, location, flags) {
}

KeyEvent::KeyEvent(const ui::NativeEvent& native_event)
    : Event(native_event,
            ui::EventTypeFromNative(native_event),
            ui::EventFlagsFromNative(native_event)),
      key_code_(ui::KeyboardCodeFromNative(native_event)) {
}

KeyEvent::KeyEvent(ui::EventType type,
                   ui::KeyboardCode key_code,
                   int flags)
    : Event(type, flags),
      key_code_(key_code) {
}

}  // namespace aura

