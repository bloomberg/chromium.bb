// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include "base/logging.h"
#include "ui/aura/event.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(const NativeEvent& native_event)
    : Event(native_event, native_event->type(), native_event->flags()),
      location_(static_cast<aura::LocatedEvent*>(native_event)->location()) {
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(const NativeEvent& native_event)
    : Event(native_event, native_event->type(), native_event->flags()),
      key_code_(static_cast<aura::KeyEvent*>(native_event)->key_code()),
      character_(0),
      unmodified_character_(0) {
}

uint16 KeyEvent::GetCharacter() const {
  NOTIMPLEMENTED();
  return key_code_;
}

uint16 KeyEvent::GetUnmodifiedCharacter() const {
  NOTIMPLEMENTED();
  return key_code_;
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(const NativeEvent& native_event)
    : MouseEvent(native_event),
      offset_(0 /* TODO(beng): obtain */) {
}

}  // namespace views
