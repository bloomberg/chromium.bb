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

const int kRepeatDelayMs = 500;
const int kRepeatIntervalMs = 50;

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
      keyboard_layout_engine_(keyboard_layout_engine),
      repeat_enabled_(true),
      repeat_key_(KEY_RESERVED) {
  repeat_delay_ = base::TimeDelta::FromMilliseconds(kRepeatDelayMs);
  repeat_interval_ = base::TimeDelta::FromMilliseconds(kRepeatIntervalMs);
}

KeyboardEvdev::~KeyboardEvdev() {
}

void KeyboardEvdev::OnKeyChange(unsigned int key, bool down) {
  if (key > KEY_MAX)
    return;

  if (down == key_state_.test(key))
    return;

  // State transition: !(down) -> (down)
  if (down)
    key_state_.set(key);
  else
    key_state_.reset(key);

  UpdateModifier(key, down);
  UpdateKeyRepeat(key, down);
  DispatchKey(key, down, false /* repeat */);
}

bool KeyboardEvdev::IsAutoRepeatEnabled() {
  return repeat_enabled_;
}

void KeyboardEvdev::SetAutoRepeatEnabled(bool enabled) {
  repeat_enabled_ = enabled;
}

void KeyboardEvdev::SetAutoRepeatRate(const base::TimeDelta& delay,
                                      const base::TimeDelta& interval) {
  repeat_delay_ = delay;
  repeat_interval_ = interval;
}

void KeyboardEvdev::GetAutoRepeatRate(base::TimeDelta* delay,
                                      base::TimeDelta* interval) {
  *delay = repeat_delay_;
  *interval = repeat_interval_;
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

void KeyboardEvdev::UpdateKeyRepeat(unsigned int key, bool down) {
  if (!repeat_enabled_)
    StopKeyRepeat();
  else if (key != repeat_key_ && down)
    StartKeyRepeat(key);
  else if (key == repeat_key_ && !down)
    StopKeyRepeat();
}

void KeyboardEvdev::StartKeyRepeat(unsigned int key) {
  repeat_key_ = key;
  repeat_delay_timer_.Start(
      FROM_HERE, repeat_delay_,
      base::Bind(&KeyboardEvdev::OnRepeatDelayTimeout, base::Unretained(this)));
  repeat_interval_timer_.Stop();
}

void KeyboardEvdev::StopKeyRepeat() {
  repeat_key_ = KEY_RESERVED;
  repeat_delay_timer_.Stop();
  repeat_interval_timer_.Stop();
}

void KeyboardEvdev::OnRepeatDelayTimeout() {
  DispatchKey(repeat_key_, true /* down */, true /* repeat */);

  repeat_interval_timer_.Start(
      FROM_HERE, repeat_interval_,
      base::Bind(&KeyboardEvdev::OnRepeatIntervalTimeout,
                 base::Unretained(this)));
}

void KeyboardEvdev::OnRepeatIntervalTimeout() {
  DispatchKey(repeat_key_, true /* down */, true /* repeat */);
}

void KeyboardEvdev::DispatchKey(unsigned int key, bool down, bool repeat) {
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
  uint32 platform_keycode = 0;
  if (!keyboard_layout_engine_->Lookup(dom_code, flags, &dom_key, &character,
                                       &key_code, &platform_keycode)) {
    return;
  }

  KeyEvent* event =
      new KeyEvent(down ? ET_KEY_PRESSED : ET_KEY_RELEASED, key_code, dom_code,
                   flags, dom_key, character);
  if (platform_keycode)
    event->set_platform_keycode(platform_keycode);
  callback_.Run(make_scoped_ptr(event));
}

// static
int KeyboardEvdev::NativeCodeToEvdevCode(int native_code) {
  if (native_code == KeycodeConverter::InvalidNativeKeycode()) {
    return KEY_RESERVED;
  }
  return native_code - kXkbKeycodeOffset;
}

}  // namespace ui
