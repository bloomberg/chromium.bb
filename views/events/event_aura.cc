// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include "aura/event.h"
#include "base/logging.h"

namespace views {

namespace {

int GetKeyStateFlags() {
  NOTIMPLEMENTED();
  return 0;
}

ui::EventType EventTypeFromNative(NativeEvent native_event) {
  return native_event->type();
}

int EventFlagsFromNative(NativeEvent native_event) {
  return native_event->flags();
}

}

////////////////////////////////////////////////////////////////////////////////
// Event, private:

void Event::Init() {
}

void Event::InitWithNativeEvent(NativeEvent native_event) {
  native_event_ = native_event;
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  native_event_2_ = NULL;
}

void Event::InitWithNativeEvent2(NativeEvent2 native_event_2,
                                 FromNativeEvent2) {
  // No one should ever call this on Aura.
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
  native_event_2_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(NativeEvent native_event)
    : Event(native_event, native_event->type(), native_event->flags()),
      location_(static_cast<aura::LocatedEvent*>(native_event)->location()) {
}

LocatedEvent::LocatedEvent(NativeEvent2 native_event_2,
                           FromNativeEvent2 from_native)
    : Event(native_event_2, ui::ET_UNKNOWN, 0, from_native) {
  // No one should ever call this on Windows.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(NativeEvent native_event)
    : Event(native_event, native_event->type(), GetKeyStateFlags()),
      key_code_(static_cast<aura::KeyEvent*>(native_event)->key_code()),
      character_(0),
      unmodified_character_(0) {
}

KeyEvent::KeyEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native)
    : Event(native_event_2, ui::ET_UNKNOWN, 0, from_native) {
  // No one should ever call this on Windows.
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
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
// MouseEvent, public:

MouseEvent::MouseEvent(NativeEvent native_event)
    : LocatedEvent(native_event) {
}

MouseEvent::MouseEvent(NativeEvent2 native_event_2,
                       FromNativeEvent2 from_native)
    : LocatedEvent(native_event_2, from_native) {
  // No one should ever call this on Windows.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(NativeEvent native_event)
    : MouseEvent(native_event),
      offset_(0 /* TODO(beng): obtain */) {
}

MouseWheelEvent::MouseWheelEvent(NativeEvent2 native_event_2,
                                 FromNativeEvent2 from_native)
    : MouseEvent(native_event_2, from_native) {
  // No one should ever call this on Windows.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}

}  // namespace views
