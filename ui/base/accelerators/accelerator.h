// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  Accelerator();
  Accelerator(ui::KeyboardCode keycode, int modifiers);
  Accelerator(const Accelerator& accelerator);
  ~Accelerator();

  Accelerator& operator=(const Accelerator& accelerator);

  // We define the < operator so that the KeyboardShortcut can be used as a key
  // in a std::map.
  bool operator <(const Accelerator& rhs) const;

  bool operator ==(const Accelerator& rhs) const;

  bool operator !=(const Accelerator& rhs) const;

  ui::KeyboardCode key_code() const { return key_code_; }

  // Sets the event type if the accelerator should be processed on an event
  // other than ui::ET_KEY_PRESSED.
  void set_type(ui::EventType type) { type_ = type; }
  ui::EventType type() const { return type_; }

  int modifiers() const { return modifiers_; }

  bool IsShiftDown() const;
  bool IsCtrlDown() const;
  bool IsAltDown() const;

  // Returns a string with the localized shortcut if any.
  string16 GetShortcutText() const;

 protected:
  // The keycode (VK_...).
  KeyboardCode key_code_;

  // The event type (usually ui::ET_KEY_PRESSED).
  EventType type_;

  // The state of the Shift/Ctrl/Alt keys (platform-dependent).
  int modifiers_;
};

// An interface that classes that want to register for keyboard accelerators
// should implement.
class UI_EXPORT AcceleratorTarget {
 public:
  // Should return true if the accelerator was processed.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) = 0;

  // Should return true if the target can handle the accelerator events. The
  // AcceleratorPressed method is inovked only for targets for which
  // CanHandleAccelerators returns true.
  virtual bool CanHandleAccelerators() const = 0;

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
