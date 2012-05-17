// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#endif

#include <cstring>

#include "ui/aura/window.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/point3.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/transform.h"

#if defined(OS_MACOSX)
#include "ui/aura/event_mac.h"
#endif

#if defined(USE_X11)
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#endif

namespace {

base::NativeEvent CopyNativeEvent(const base::NativeEvent& event) {
#if defined(USE_X11)
  XEvent* copy = new XEvent;
  *copy = *event;
  return copy;
#elif defined(OS_WIN)
  return event;
#elif defined(OS_MACOSX)
  return aura::CopyNativeEvent(event);
#else
  NOTREACHED() <<
      "Don't know how to copy base::NativeEvent for this platform";
#endif
}

}  // namespace

namespace aura {

Event::~Event() {
#if defined(USE_X11)
  if (delete_native_event_)
    delete native_event_;
#endif
}

bool Event::HasNativeEvent() const {
  base::NativeEvent null_event;
  std::memset(&null_event, 0, sizeof(null_event));
  return !!std::memcmp(&native_event_, &null_event, sizeof(null_event));
}

Event::Event(ui::EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime() - base::Time()),
      flags_(flags),
      delete_native_event_(false) {
  Init();
}

Event::Event(const base::NativeEvent& native_event,
             ui::EventType type,
             int flags)
    : type_(type),
      time_stamp_(ui::EventTimeFromNative(native_event)),
      flags_(flags),
      delete_native_event_(false) {
  InitWithNativeEvent(native_event);
}

Event::Event(const Event& copy)
    : native_event_(copy.native_event_),
      type_(copy.type_),
      time_stamp_(copy.time_stamp_),
      flags_(copy.flags_),
      delete_native_event_(false) {
}

void Event::Init() {
  std::memset(&native_event_, 0, sizeof(native_event_));
}

void Event::InitWithNativeEvent(const base::NativeEvent& native_event) {
  native_event_ = native_event;
}

LocatedEvent::~LocatedEvent() {
}

LocatedEvent::LocatedEvent(const base::NativeEvent& native_event)
    : Event(native_event,
            ui::EventTypeFromNative(native_event),
            ui::EventFlagsFromNative(native_event)),
      location_(ui::EventLocationFromNative(native_event)),
      root_location_(location_) {
}

LocatedEvent::LocatedEvent(const LocatedEvent& model,
                           Window* source,
                           Window* target)
    : Event(model),
      location_(model.location_),
      root_location_(model.root_location_) {
  if (target && target != source)
    Window::ConvertPointToWindow(source, target, &location_);
}

LocatedEvent::LocatedEvent(ui::EventType type,
                           const gfx::Point& location,
                           const gfx::Point& root_location,
                           int flags)
    : Event(type, flags),
      location_(location),
      root_location_(location) {
}

void LocatedEvent::UpdateForRootTransform(const ui::Transform& root_transform) {
  // Transform has to be done at root level.
  DCHECK_EQ(root_location_.x(), location_.x());
  DCHECK_EQ(root_location_.y(), location_.y());
  gfx::Point3f p(location_);
  root_transform.TransformPointReverse(p);
  root_location_ = location_ = p.AsPoint();
}

MouseEvent::MouseEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event) {
  if (type() == ui::ET_MOUSE_PRESSED)
    SetClickCount(GetRepeatCount(*this));
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
                       const gfx::Point& root_location,
                       int flags)
    : LocatedEvent(type, location, root_location, flags) {
}

// static
bool MouseEvent::IsRepeatedClickEvent(
    const MouseEvent& event1,
    const MouseEvent& event2) {
  // These values match the Windows defaults.
  static const int kDoubleClickTimeMS = 500;
  static const int kDoubleClickWidth = 4;
  static const int kDoubleClickHeight = 4;

  if (event1.type() != ui::ET_MOUSE_PRESSED ||
      event2.type() != ui::ET_MOUSE_PRESSED)
    return false;

  // Compare flags, but ignore EF_IS_DOUBLE_CLICK to allow triple clicks.
  if ((event1.flags() & ~ui::EF_IS_DOUBLE_CLICK) !=
      (event2.flags() & ~ui::EF_IS_DOUBLE_CLICK))
    return false;

  base::TimeDelta time_difference = event2.time_stamp() - event1.time_stamp();

  if (time_difference.InMilliseconds() > kDoubleClickTimeMS)
    return false;

  if (abs(event2.x() - event1.x()) > kDoubleClickWidth / 2)
    return false;

  if (abs(event2.y() - event1.y()) > kDoubleClickHeight / 2)
    return false;

  return true;
}

// static
int MouseEvent::GetRepeatCount(const MouseEvent& event) {
  int click_count = 1;
  if (last_click_event_) {
    if (IsRepeatedClickEvent(*last_click_event_, event))
      click_count = last_click_event_->GetClickCount() + 1;
    delete last_click_event_;
  }
  last_click_event_ = new MouseEvent(event, NULL, NULL);
  if (click_count > 3)
    click_count = 3;
  last_click_event_->SetClickCount(click_count);
  return click_count;
}

// static
MouseEvent* MouseEvent::last_click_event_ = NULL;

int MouseEvent::GetClickCount() const {
  if (type() != ui::ET_MOUSE_PRESSED)
    return 0;

  if (flags() & ui::EF_IS_TRIPLE_CLICK)
    return 3;
  else if (flags() & ui::EF_IS_DOUBLE_CLICK)
    return 2;
  else
    return 1;
}

void MouseEvent::SetClickCount(int click_count) {
  if (type() != ui::ET_MOUSE_PRESSED)
    return;

  DCHECK(click_count > 0);
  DCHECK(click_count <= 3);

  int f = flags();
  switch (click_count) {
    case 1:
      f &= ~ui::EF_IS_DOUBLE_CLICK;
      f &= ~ui::EF_IS_TRIPLE_CLICK;
      break;
    case 2:
      f |= ui::EF_IS_DOUBLE_CLICK;
      f &= ~ui::EF_IS_TRIPLE_CLICK;
      break;
    case 3:
      f &= ~ui::EF_IS_DOUBLE_CLICK;
      f |= ui::EF_IS_TRIPLE_CLICK;
      break;
  }
  set_flags(f);
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
                       int touch_id,
                       base::TimeDelta time_stamp)
    : LocatedEvent(type, location, location, 0),
      touch_id_(touch_id),
      radius_x_(1.0f),
      radius_y_(1.0f),
      rotation_angle_(0.0f),
      force_(0.0f) {
  set_time_stamp(time_stamp);
}

TouchEvent::~TouchEvent() {
}

void TouchEvent::UpdateForRootTransform(const ui::Transform& root_transform) {
  LocatedEvent::UpdateForRootTransform(root_transform);
  gfx::Point3f scale;
  ui::InterpolatedTransform::FactorTRS(root_transform, NULL, NULL, &scale);
  radius_x_ *= scale.x();
  radius_y_ *= scale.y();
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
  return time_stamp();
}

TouchEvent* TouchEvent::Copy() const {
#if defined(OS_WIN)
  if (native_event().message)
    return new TouchEvent(::CopyNativeEvent(native_event()));
#endif
  return new TouchEvent(*this, NULL, NULL);
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

  uint16 ch = 0;
  if (!IsControlDown())
    ch = ui::GetCharacterFromXEvent(native_event());
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

  static const unsigned int kIgnoredModifiers = ControlMask | LockMask |
      Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask;

  XKeyEvent copy = native_event()->xkey;  // bit-wise copy is safe.
  // We can't use things like (native_event()->xkey.state & ShiftMask), as it
  // may mask out bits used by X11 internally.
  copy.state &= ~kIgnoredModifiers;
  uint16 ch = ui::GetCharacterFromXEvent(reinterpret_cast<XEvent*>(&copy));
  return ch ? ch :
      ui::GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
#else
  NOTIMPLEMENTED();
  return 0;
#endif
}

KeyEvent* KeyEvent::Copy() {
  KeyEvent* copy = new KeyEvent(::CopyNativeEvent(native_event()), is_char());
#if defined(USE_X11)
  copy->set_delete_native_event(true);
#endif
  return copy;
}

TranslatedKeyEvent::TranslatedKeyEvent(const base::NativeEvent& native_event,
                                       bool is_char)
    : KeyEvent(native_event, is_char) {
  set_type(type() == ui::ET_KEY_PRESSED ?
           ui::ET_TRANSLATED_KEY_PRESS : ui::ET_TRANSLATED_KEY_RELEASE);
}

TranslatedKeyEvent::TranslatedKeyEvent(bool is_press,
                                       ui::KeyboardCode key_code,
                                       int flags)
    : KeyEvent((is_press ?
                ui::ET_TRANSLATED_KEY_PRESS : ui::ET_TRANSLATED_KEY_RELEASE),
               key_code,
               flags) {
}

void TranslatedKeyEvent::ConvertToKeyEvent() {
  set_type(type() == ui::ET_TRANSLATED_KEY_PRESS ?
           ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED);
}

ScrollEvent::ScrollEvent(const base::NativeEvent& native_event)
    : MouseEvent(native_event) {
  if (type() == ui::ET_SCROLL) {
    ui::GetScrollOffsets(native_event, &x_offset_, &y_offset_);
    double start, end;
    ui::GetGestureTimes(native_event, &start, &end);
  } else if (type() == ui::ET_SCROLL_FLING_START) {
    bool is_cancel;
    ui::GetFlingData(native_event, &x_offset_, &y_offset_, &is_cancel);
  }
}

GestureEvent::GestureEvent(ui::EventType type,
                           int x,
                           int y,
                           int flags,
                           base::Time time_stamp,
                           float delta_x,
                           float delta_y,
                           unsigned int touch_ids_bitfield)
    : LocatedEvent(type, gfx::Point(x, y), gfx::Point(x, y), flags),
      delta_x_(delta_x),
      delta_y_(delta_y),
      touch_ids_bitfield_(touch_ids_bitfield) {
  set_time_stamp(base::TimeDelta::FromSeconds(time_stamp.ToDoubleT()));
}

GestureEvent::GestureEvent(const GestureEvent& model,
                           Window* source,
                           Window* target)
    : LocatedEvent(model, source, target),
      delta_x_(model.delta_x_),
      delta_y_(model.delta_y_),
      touch_ids_bitfield_(model.touch_ids_bitfield_) {
}

GestureEvent::~GestureEvent() {
}

int GestureEvent::GetLowestTouchId() const {
  if (touch_ids_bitfield_ == 0)
    return -1;
  int i = -1;
  // Find the index of the least significant 1 bit
  while (!(1 << ++i & touch_ids_bitfield_));
  return i;
}

}  // namespace aura
