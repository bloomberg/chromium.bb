// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"
#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/input_injector_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"

namespace ui {

InputInjectorEvdev::InputInjectorEvdev(EventModifiersEvdev* modifiers,
                                       CursorDelegateEvdev* cursor,
                                       KeyboardEvdev* keyboard,
                                       const EventDispatchCallback& callback)
    : modifiers_(modifiers),
      cursor_(cursor),
      keyboard_(keyboard),
      callback_(callback) {
  DCHECK(modifiers_);
  DCHECK(cursor_);
  DCHECK(keyboard_);
}

InputInjectorEvdev::~InputInjectorEvdev() {
}

void InputInjectorEvdev::InjectMouseButton(EventFlags button, bool down) {
  int changed_button = 0;

  switch(button) {
    case EF_LEFT_MOUSE_BUTTON:
      changed_button = EVDEV_MODIFIER_LEFT_MOUSE_BUTTON;
      break;
    case EF_RIGHT_MOUSE_BUTTON:
      changed_button = EVDEV_MODIFIER_RIGHT_MOUSE_BUTTON;
      break;
    case EF_MIDDLE_MOUSE_BUTTON:
      changed_button = EVDEV_MODIFIER_MIDDLE_MOUSE_BUTTON;
    default:
      LOG(WARNING) << "Invalid flag: " << button << " for the button parameter";
      return;
  }

  modifiers_->UpdateModifier(changed_button, down);
  int changed_button_flag =
      EventModifiersEvdev::GetEventFlagFromModifier(changed_button);
  callback_.Run(make_scoped_ptr(new MouseEvent(
      (down) ? ET_MOUSE_PRESSED : ET_MOUSE_RELEASED,
      cursor_->GetLocation(),
      cursor_->GetLocation(),
      modifiers_->GetModifierFlags() | changed_button_flag,
      changed_button_flag)));
}

void InputInjectorEvdev::InjectMouseWheel(int delta_x, int delta_y) {
  callback_.Run(make_scoped_ptr(new MouseWheelEvent(
      gfx::Vector2d(delta_x, delta_y),
      cursor_->GetLocation(),
      cursor_->GetLocation(),
      modifiers_->GetModifierFlags(),
      0 /* changed_button_flags */)));
}

void InputInjectorEvdev::MoveCursorTo(const gfx::PointF& location) {
  if (cursor_) {
    cursor_->MoveCursorTo(location);
    callback_.Run(make_scoped_ptr(new MouseEvent(
        ET_MOUSE_MOVED,
        cursor_->GetLocation(),
        cursor_->GetLocation(),
        modifiers_->GetModifierFlags(),
        0 /* changed_button_flags */)));
  }
}

void InputInjectorEvdev::InjectKeyPress(DomCode physical_key, bool down) {
  if (physical_key == DomCode::NONE) {
    return;
  }

  int native_keycode = KeycodeConverter::DomCodeToNativeKeycode(physical_key);
  int evdev_code = KeyboardEvdev::NativeCodeToEvdevCode(native_keycode);
  keyboard_->OnKeyChange(evdev_code, down);
}

}  // namespace ui

