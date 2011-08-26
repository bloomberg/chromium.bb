// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include <X11/Xlib.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/wayland/events/wayland_event.h"

namespace views {

namespace {

static int kWheelScrollAmount = 53;

ui::EventType EventTypeFromNative(NativeEvent native_event) {
  switch (native_event->type) {
    case ui::WAYLAND_BUTTON:
      switch (native_event->button.button) {
        case ui::LEFT_BUTTON:
        case ui::RIGHT_BUTTON:
        case ui::MIDDLE_BUTTON:
          return native_event->button.state ? ui::ET_MOUSE_PRESSED
                                            : ui::ET_MOUSE_RELEASED;
        case ui::SCROLL_UP:
        case ui::SCROLL_DOWN:
          return ui::ET_MOUSEWHEEL;
        default:
          break;
      }
      break;
    case ui::WAYLAND_KEY:
      return native_event->key.state ? ui::ET_KEY_PRESSED
                                     : ui::ET_KEY_RELEASED;
    case ui::WAYLAND_MOTION:
      return ui::ET_MOUSE_MOVED;
    case ui::WAYLAND_POINTER_FOCUS:
      return native_event->pointer_focus.state ? ui::ET_MOUSE_ENTERED
                                               : ui::ET_MOUSE_EXITED;
    case ui::WAYLAND_KEYBOARD_FOCUS:
      return ui::ET_UNKNOWN;
    default:
      break;
  }
  return ui::ET_UNKNOWN;
}

gfx::Point GetMouseEventLocation(NativeEvent native_event) {
  switch (native_event->type) {
    case ui::WAYLAND_BUTTON:
      return gfx::Point(native_event->button.x, native_event->button.y);
    case ui::WAYLAND_MOTION:
      return gfx::Point(native_event->motion.x, native_event->motion.y);
    case ui::WAYLAND_POINTER_FOCUS:
      return gfx::Point(native_event->pointer_focus.x,
                        native_event->pointer_focus.y);
    default:
      return gfx::Point(0, 0);
  }
}

int GetEventFlagsFromState(unsigned int state) {
  int flags = 0;
  if (state & ControlMask)
    flags |= ui::EF_CONTROL_DOWN;
  if (state & ShiftMask)
    flags |= ui::EF_SHIFT_DOWN;
  if (state & Mod1Mask)
    flags |= ui::EF_ALT_DOWN;
  if (state & LockMask)
    flags |= ui::EF_CAPS_LOCK_DOWN;
  if (state & Button1Mask)
    flags |= ui::EF_LEFT_BUTTON_DOWN;
  if (state & Button2Mask)
    flags |= ui::EF_MIDDLE_BUTTON_DOWN;
  if (state & Button3Mask)
    flags |= ui::EF_RIGHT_BUTTON_DOWN;

  return flags;
}

int GetButtonEventFlagsFromNativeEvent(NativeEvent native_event) {
  // TODO(dnicoara): Need to add double click.
  int flags = 0;
  switch (native_event->button.button) {
    case ui::LEFT_BUTTON:
      return flags | ui::EF_LEFT_BUTTON_DOWN;
    case ui::MIDDLE_BUTTON:
      return flags | ui::EF_MIDDLE_BUTTON_DOWN;
    case ui::RIGHT_BUTTON:
      return flags | ui::EF_RIGHT_BUTTON_DOWN;
  }
  return flags;
}

int GetEventFlagsFromNativeEvent(NativeEvent native_event) {
  switch (native_event->type) {
    case ui::WAYLAND_BUTTON:
      return GetButtonEventFlagsFromNativeEvent(native_event) |
             GetEventFlagsFromState(native_event->button.modifiers);
    case ui::WAYLAND_KEY:
      return GetEventFlagsFromState(native_event->key.modifiers);
    case ui::WAYLAND_MOTION:
      return GetEventFlagsFromState(native_event->motion.modifiers);
    case ui::WAYLAND_KEYBOARD_FOCUS:
      return GetEventFlagsFromState(native_event->keyboard_focus.modifiers);
    default:
      return 0;
  }
}

int GetMouseWheelOffset(NativeEvent native_event) {
  if (native_event->button.button == ui::SCROLL_UP) {
    return kWheelScrollAmount;
  } else {
    return -kWheelScrollAmount;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Event, private:

void Event::Init() {
  native_event_ = NULL;
  native_event_2_ = NULL;
}

void Event::InitWithNativeEvent(NativeEvent native_event) {
  native_event_ = native_event;
  // TODO(dnicoara): Remove once we rid views of Gtk/Gdk.
  native_event_2_ = NULL;
}

void Event::InitWithNativeEvent2(NativeEvent2 native_event_2,
                                 FromNativeEvent2) {
  native_event_2_ = NULL;
  // TODO(dnicoara): Remove once we rid views of Gtk/Gdk.
  native_event_2_ = native_event_2;
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(NativeEvent native_event)
    : LocatedEvent(native_event) {
}

MouseEvent::MouseEvent(NativeEvent2 native_event_2,
                       FromNativeEvent2 from_native)
    : LocatedEvent(native_event_2, from_native) {
  // TODO(dnicoara): Remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(NativeEvent native_event)
    : Event(native_event, EventTypeFromNative(native_event),
            GetEventFlagsFromNativeEvent(native_event)),
      location_(GetMouseEventLocation(native_event)) {
}

LocatedEvent::LocatedEvent(NativeEvent2 native_event_2,
                           FromNativeEvent2 from_native)
    : Event(native_event_2, ui::ET_UNKNOWN, 0, from_native) {
  // TODO(dnicoara) Remove once we rid views of Gtk.
  NOTREACHED();
}

//////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:


KeyEvent::KeyEvent(NativeEvent native_event)
    : Event(native_event, EventTypeFromNative(native_event),
            GetEventFlagsFromNativeEvent(native_event)),
      key_code_(ui::KeyboardCodeFromXKeysym(native_event->key.sym)) {
}

KeyEvent::KeyEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native)
    : Event(native_event_2, ui::ET_UNKNOWN, 0, from_native) {
  // TODO(dnicoara) Remove once we rid views of Gtk.
  NOTREACHED();
}

uint16 KeyEvent::GetCharacter() const {
  return GetCharacterFromKeyCode(key_code_, flags());
}

uint16 KeyEvent::GetUnmodifiedCharacter() const {
  return GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
}

//////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(NativeEvent native_event)
    : MouseEvent(native_event),
      offset_(GetMouseWheelOffset(native_event)) {
}

MouseWheelEvent::MouseWheelEvent(NativeEvent2 native_event_2,
                                 FromNativeEvent2 from_native)
    : MouseEvent(native_event_2, from_native) {
  // TODO(dnicoara) Remove once we rid views of Gtk.
  NOTREACHED();
}

}  // namespace views
