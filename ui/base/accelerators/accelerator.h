// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class describe a keyboard accelerator (or keyboard shortcut).
// Keyboard accelerators are registered with the FocusManager.
// It has a copy constructor and assignment operator so that it can be copied.
// It also defines the < operator so that it can be used as a key in a std::map.
//

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_H_
#pragma once

#include "base/string16.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/events.h"
#include "ui/base/ui_export.h"

namespace ui {

// This is a cross-platform base class for accelerator keys used in menus. It is
// meant to be subclassed for concrete toolkit implementations.
class UI_EXPORT Accelerator {
 public:
  Accelerator() : key_code_(ui::VKEY_UNKNOWN), modifiers_(0) {}

  Accelerator(ui::KeyboardCode keycode, int modifiers)
      : key_code_(keycode),
        modifiers_(modifiers) {}

  Accelerator(const Accelerator& accelerator) {
    key_code_ = accelerator.key_code_;
    modifiers_ = accelerator.modifiers_;
  }

  Accelerator(ui::KeyboardCode keycode,
              bool shift_pressed, bool ctrl_pressed, bool alt_pressed)
      : key_code_(keycode),
        modifiers_(0) {
    if (shift_pressed)
      modifiers_ |= ui::EF_SHIFT_DOWN;
    if (ctrl_pressed)
      modifiers_ |= ui::EF_CONTROL_DOWN;
    if (alt_pressed)
      modifiers_ |= ui::EF_ALT_DOWN;
  }

  virtual ~Accelerator() {}

  Accelerator& operator=(const Accelerator& accelerator) {
    if (this != &accelerator) {
      key_code_ = accelerator.key_code_;
      modifiers_ = accelerator.modifiers_;
    }
    return *this;
  }

  // We define the < operator so that the KeyboardShortcut can be used as a key
  // in a std::map.
  bool operator <(const Accelerator& rhs) const {
    if (key_code_ != rhs.key_code_)
      return key_code_ < rhs.key_code_;
    return modifiers_ < rhs.modifiers_;
  }

  bool operator ==(const Accelerator& rhs) const {
    return (key_code_ == rhs.key_code_) && (modifiers_ == rhs.modifiers_);
  }

  bool operator !=(const Accelerator& rhs) const {
    return !(*this == rhs);
  }

  ui::KeyboardCode key_code() const { return key_code_; }

  int modifiers() const { return modifiers_; }

  bool IsShiftDown() const {
    return (modifiers_ & ui::EF_SHIFT_DOWN) == ui::EF_SHIFT_DOWN;
  }

  bool IsCtrlDown() const {
    return (modifiers_ & ui::EF_CONTROL_DOWN) == ui::EF_CONTROL_DOWN;
  }

  bool IsAltDown() const {
    return (modifiers_ & ui::EF_ALT_DOWN) == ui::EF_ALT_DOWN;
  }

  // Returns a string with the localized shortcut if any.
  string16 GetShortcutText() const;

 protected:
  // The keycode (VK_...).
  ui::KeyboardCode key_code_;

  // The state of the Shift/Ctrl/Alt keys (platform-dependent).
  int modifiers_;
};

// An interface that classes that want to register for keyboard accelerators
// should implement.
class UI_EXPORT AcceleratorTarget {
 public:
  // This method should return true if the accelerator was processed.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) = 0;

 protected:
  virtual ~AcceleratorTarget() {}
};

// Since acclerator code is one of the few things that can't be cross platform
// in the chrome UI, separate out just the GetAcceleratorForCommandId() from
// the menu delegates.
class AcceleratorProvider {
 public:
  // Gets the accelerator for the specified command id. Returns true if the
  // command id has a valid accelerator, false otherwise.
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator) = 0;

 protected:
  virtual ~AcceleratorProvider() {}
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_H_
