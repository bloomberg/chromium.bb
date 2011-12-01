// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#endif

#include "ui/aura/window.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/point3.h"
#include "ui/gfx/transform.h"

#if defined(USE_X11)
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#endif

#if !defined(OS_WIN)
namespace {

// On non-windows systems, double-click events aren't reported by the system.
// So aura has to detect double-clicks itself.
double g_last_click_time = 0.0;
int g_last_click_x = 0;
int g_last_click_y = 0;
int g_flags = 0;

void RememberClickForDoubleClickDetection(const aura::MouseEvent& event) {
  g_last_click_time = event.time_stamp().ToDoubleT();
  g_last_click_x = event.location().x();
  g_last_click_y = event.location().y();
  g_flags = event.flags();
}

bool IsDoubleClick(const aura::MouseEvent& event) {
  // The flags must be the same
  if ((g_flags & event.flags()) != g_flags)
    return false;

  const int double_click_distance = 5;
  const double double_click_time = 0.250;  // in seconds
  return std::abs(event.location().x() - g_last_click_x) <=
           double_click_distance &&
         std::abs(event.location().y() - g_last_click_y) <=
           double_click_distance &&
         event.time_stamp().ToDoubleT() - g_last_click_time <=
           double_click_time;
}

}  // namespace
#endif  // !defined(OS_WIN)

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
#if !defined(OS_WIN)
  if (type() == ui::ET_MOUSE_PRESSED) {
    if (IsDoubleClick(*this))
      set_flags(flags() | ui::EF_IS_DOUBLE_CLICK);
    else
      RememberClickForDoubleClickDetection(*this);
  }
#endif
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
      is_char_(is_char),
      character_(0),
      unmodified_character_(0) {
}

KeyEvent::KeyEvent(ui::EventType type,
                   ui::KeyboardCode key_code,
                   int flags)
    : Event(type, flags),
      key_code_(key_code),
      is_char_(false),
      character_(ui::GetCharacterFromKeyCode(key_code, flags)),
      unmodified_character_(0) {
}

uint16 KeyEvent::GetCharacter() const {
  if (character_)
    return character_;

#if defined(OS_WIN)
  return (native_event().message == WM_CHAR) ? key_code_ :
      ui::GetCharacterFromKeyCode(key_code_, flags());
#elif defined(USE_X11)
  if (!native_event())
    return ui::GetCharacterFromKeyCode(key_code_, flags());

  DCHECK(native_event()->type == KeyPress ||
         native_event()->type == KeyRelease);

  uint16 ch = ui::DefaultSymbolFromXEvent(native_event());
  return ch ? ch : ui::GetCharacterFromKeyCode(key_code_, flags());
#else
  NOTIMPLEMENTED();
  return 0;
#endif
}

uint16 KeyEvent::GetUnmodifiedCharacter() const {
  if (unmodified_character_)
    return unmodified_character_;

#if defined(OS_WIN)
  // Looks like there is no way to get unmodified character on Windows.
  return (native_event().message == WM_CHAR) ? key_code_ :
      ui::GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
#elif defined(USE_X11)
  if (!native_event())
    return ui::GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);

  DCHECK(native_event()->type == KeyPress ||
         native_event()->type == KeyRelease);

  XKeyEvent *key = &native_event()->xkey;

  static const unsigned int kIgnoredModifiers = ControlMask | LockMask |
      Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask;

  // We can't use things like (key.state & ShiftMask), as it may mask out bits
  // used by X11 internally.
  key->state &= ~kIgnoredModifiers;
  uint16 ch = ui::DefaultSymbolFromXEvent(native_event());
  return ch ? ch :
      ui::GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
#else
  NOTIMPLEMENTED();
  return 0;
#endif
}

}  // namespace aura
