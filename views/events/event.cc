// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include "views/view.h"
#include "views/widget/root_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Event, protected:

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

Event::Event(NativeEvent2 native_event_2, ui::EventType type, int flags,
             FromNativeEvent2 from_native)
    : native_event_2_(native_event_2),
      type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  InitWithNativeEvent2(native_event_2, from_native);
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

// TODO(msw): Kill this legacy constructor when we update uses.
LocatedEvent::LocatedEvent(ui::EventType type, const gfx::Point& location,
                           int flags)
    : Event(type, flags),
      location_(location) {
}

LocatedEvent::LocatedEvent(const LocatedEvent& model, View* source,
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

KeyEvent::KeyEvent(ui::EventType type, ui::KeyboardCode key_code,
                   int event_flags)
    : Event(type, event_flags),
      key_code_(key_code) {
}

// KeyEvent, private: ---------------------------------------------------------

// static
uint16 KeyEvent::GetCharacterFromKeyCode(ui::KeyboardCode key_code, int flags) {
  const bool ctrl = (flags & ui::EF_CONTROL_DOWN) != 0;
  const bool shift = (flags & ui::EF_SHIFT_DOWN) != 0;
  const bool upper = shift ^ ((flags & ui::EF_CAPS_LOCK_DOWN) != 0);

  // Following Windows behavior to map ctrl-a ~ ctrl-z to \x01 ~ \x1A.
  if (key_code >= ui::VKEY_A && key_code <= ui::VKEY_Z)
    return key_code - ui::VKEY_A + (ctrl ? 1 : (upper ? 'A' : 'a'));

  // Other ctrl characters
  if (ctrl) {
    if (shift) {
      // following graphics chars require shift key to input.
      switch (key_code) {
        // ctrl-@ maps to \x00 (Null byte)
        case ui::VKEY_2:
          return 0;
        // ctrl-^ maps to \x1E (Record separator, Information separator two)
        case ui::VKEY_6:
          return 0x1E;
        // ctrl-_ maps to \x1F (Unit separator, Information separator one)
        case ui::VKEY_OEM_MINUS:
          return 0x1F;
        // Returns 0 for all other keys to avoid inputting unexpected chars.
        default:
          return 0;
      }
    } else {
      switch (key_code) {
        // ctrl-[ maps to \x1B (Escape)
        case ui::VKEY_OEM_4:
          return 0x1B;
        // ctrl-\ maps to \x1C (File separator, Information separator four)
        case ui::VKEY_OEM_5:
          return 0x1C;
        // ctrl-] maps to \x1D (Group separator, Information separator three)
        case ui::VKEY_OEM_6:
          return 0x1D;
        // ctrl-Enter maps to \x0A (Line feed)
        case ui::VKEY_RETURN:
          return 0x0A;
        // Returns 0 for all other keys to avoid inputting unexpected chars.
        default:
          return 0;
      }
    }
  }

  // Normal characters
  if (key_code >= ui::VKEY_0 && key_code <= ui::VKEY_9)
    return shift ? ")!@#$%^&*("[key_code - ui::VKEY_0] : key_code;
  else if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9)
    return key_code - ui::VKEY_NUMPAD0 + '0';

  switch (key_code) {
    case ui::VKEY_TAB:
      return '\t';
    case ui::VKEY_RETURN:
      return '\r';
    case ui::VKEY_MULTIPLY:
      return '*';
    case ui::VKEY_ADD:
      return '+';
    case ui::VKEY_SUBTRACT:
      return '-';
    case ui::VKEY_DECIMAL:
      return '.';
    case ui::VKEY_DIVIDE:
      return '/';
    case ui::VKEY_SPACE:
      return ' ';
    case ui::VKEY_OEM_1:
      return shift ? ':' : ';';
    case ui::VKEY_OEM_PLUS:
      return shift ? '+' : '=';
    case ui::VKEY_OEM_COMMA:
      return shift ? '<' : ',';
    case ui::VKEY_OEM_MINUS:
      return shift ? '_' : '-';
    case ui::VKEY_OEM_PERIOD:
      return shift ? '>' : '.';
    case ui::VKEY_OEM_2:
      return shift ? '?' : '/';
    case ui::VKEY_OEM_3:
      return shift ? '~' : '`';
    case ui::VKEY_OEM_4:
      return shift ? '{' : '[';
    case ui::VKEY_OEM_5:
      return shift ? '|' : '\\';
    case ui::VKEY_OEM_6:
      return shift ? '}' : ']';
    case ui::VKEY_OEM_7:
      return shift ? '"' : '\'';
    default:
      return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(const MouseEvent& model, View* source, View* target)
    : LocatedEvent(model, source, target) {
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

#if defined(TOUCH_UI)
TouchEvent::TouchEvent(ui::EventType type,
                       int x,
                       int y,
                       int flags,
                       int touch_id,
                       float radius,
                       float angle,
                       float ratio)
      : LocatedEvent(type, gfx::Point(x, y), flags),
        touch_id_(touch_id),
        radius_(radius),
        angle_(angle),
        ratio_(ratio) {
}

TouchEvent::TouchEvent(const TouchEvent& model, View* source, View* target)
    : LocatedEvent(model, source, target),
      touch_id_(model.touch_id_),
      radius_(model.radius_),
      angle_(model.angle_),
      ratio_(model.ratio_) {
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, private:

TouchEvent::TouchEvent(const TouchEvent& model, View* root)
    : LocatedEvent(model, root),
      touch_id_(model.touch_id_),
      radius_(model.radius_),
      angle_(model.angle_),
      ratio_(model.ratio_) {
}
#endif

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

// This value matches windows WHEEL_DELTA.
// static
const int MouseWheelEvent::kWheelDelta = 120;

}  // namespace views
