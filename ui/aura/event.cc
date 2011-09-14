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

Event::Event(NativeEvent native_event, ui::EventType type, int flags)
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

MouseEvent::MouseEvent(const MouseEvent& model, Window* source, Window* target)
    : LocatedEvent(model, source, target) {
}

MouseEvent::MouseEvent(ui::EventType type,
                       const gfx::Point& location,
                       int flags)
    : LocatedEvent(type, location, flags) {
}

KeyEvent::KeyEvent(ui::EventType type,
                   ui::KeyboardCode key_code,
                   int flags)
    : Event(type, flags),
      key_code_(key_code) {
}

}  // namespace aura

