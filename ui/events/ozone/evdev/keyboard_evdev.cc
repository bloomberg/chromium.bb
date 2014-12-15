// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/keyboard_evdev.h"

#include "ui/events/event.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace ui {

namespace {

const int kXkbKeycodeOffset = 8;

int ModifierFromEvdevKey(unsigned int code) {
  switch (code) {
    case KEY_CAPSLOCK:
      return EVDEV_MODIFIER_CAPS_LOCK;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return EVDEV_MODIFIER_SHIFT;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return EVDEV_MODIFIER_CONTROL;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return EVDEV_MODIFIER_ALT;
    case BTN_LEFT:
      return EVDEV_MODIFIER_LEFT_MOUSE_BUTTON;
    case BTN_MIDDLE:
      return EVDEV_MODIFIER_MIDDLE_MOUSE_BUTTON;
    case BTN_RIGHT:
      return EVDEV_MODIFIER_RIGHT_MOUSE_BUTTON;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
      return EVDEV_MODIFIER_COMMAND;
    default:
      return EVDEV_MODIFIER_NONE;
  }
}

bool IsModifierLockKeyFromEvdevKey(unsigned int code) {
  return code == KEY_CAPSLOCK;
}

}  // namespace

KeyboardEvdev::KeyboardEvdev(EventModifiersEvdev* modifiers,
                             KeyboardLayoutEngine* keyboard_layout_engine,
                             const EventDispatchCallback& callback)
    : callback_(callback),
      modifiers_(modifiers),
      keyboard_layout_engine_(keyboard_layout_engine) {
}

KeyboardEvdev::~KeyboardEvdev() {
}

void KeyboardEvdev::OnKeyChange(unsigned int key, bool down) {
  if (key > KEY_MAX)
    return;

  if (down != key_state_.test(key)) {
    // State transition: !(down) -> (down)
    if (down)
      key_state_.set(key);
    else
      key_state_.reset(key);

    UpdateModifier(key, down);
  }

  DispatchKey(key, down);
}

void KeyboardEvdev::UpdateModifier(unsigned int key, bool down) {
  int modifier = ModifierFromEvdevKey(key);
  int locking = IsModifierLockKeyFromEvdevKey(key);

  if (modifier == EVDEV_MODIFIER_NONE)
    return;

  if (locking)
    modifiers_->UpdateModifierLock(modifier, down);
  else
    modifiers_->UpdateModifier(modifier, down);
}

void KeyboardEvdev::DispatchKey(unsigned int key, bool down) {
  DomCode dom_code =
      KeycodeConverter::NativeKeycodeToDomCode(key + kXkbKeycodeOffset);
  // DomCode constants are not included here because of conflicts with
  // evdev preprocessor macros.
  if (!static_cast<int>(dom_code))
    return;
  int flags = modifiers_->GetModifierFlags();
  DomKey dom_key;
  KeyboardCode key_code;
  uint16 character;
  if (!keyboard_layout_engine_->Lookup(dom_code, flags, &dom_key, &character,
                                       &key_code)) {
    return;
  }

  callback_.Run(make_scoped_ptr(
      new KeyEvent(down ? ET_KEY_PRESSED : ET_KEY_RELEASED, key_code, dom_code,
                   flags, dom_key, character)));
}

// static
int KeyboardEvdev::NativeCodeToEvdevCode(int native_code) {
  if (native_code == KeycodeConverter::InvalidNativeKeycode()) {
    return KEY_RESERVED;
  }
  return native_code - kXkbKeycodeOffset;
}

}  // namespace ui
