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

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "ui/base/accelerators/platform_accelerator.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

class KeyEvent;
class PlatformAccelerator;

// This is a cross-platform class for accelerator keys used in menus.
// |platform_accelerator| should be used to store platform specific data.
//
// While |modifiers| may include EF_IS_REPEAT, EF_IS_REPEAT is not considered
// an intrinsic part of an Accelerator. This is done so that an accelerator
// for a particular KeyEvent matches an accelerator with or without the repeat
// flag. A side effect of this is that == (and <) does not consider the
// repeat flag in its comparison.
class UI_BASE_EXPORT Accelerator {
 public:
  enum class KeyState {
    PRESSED,
    RELEASED,
  };

  Accelerator();
  // NOTE: this constructor strips out non key related flags.
  Accelerator(KeyboardCode key_code,
              int modifiers,
              KeyState key_state = KeyState::PRESSED);
  explicit Accelerator(const KeyEvent& key_event);
  Accelerator(const Accelerator& accelerator);
  ~Accelerator();

  // Masks out all the non-modifiers KeyEvent |flags| and returns only the
  // available modifier ones. This does not include EF_IS_REPEAT.
  static int MaskOutKeyEventFlags(int flags);

  KeyEvent ToKeyEvent() const;

  Accelerator& operator=(const Accelerator& accelerator);

  // Define the < operator so that the KeyboardShortcut can be used as a key in
  // a std::map.
  bool operator <(const Accelerator& rhs) const;

  bool operator ==(const Accelerator& rhs) const;

  bool operator !=(const Accelerator& rhs) const;

  KeyboardCode key_code() const { return key_code_; }

  // Sets the key state that triggers the accelerator. Default is PRESSED.
  void set_key_state(KeyState state) { key_state_ = state; }
  KeyState key_state() const { return key_state_; }

  int modifiers() const { return modifiers_; }

  bool IsShiftDown() const;
  bool IsCtrlDown() const;
  bool IsAltDown() const;
  bool IsCmdDown() const;
  bool IsRepeat() const;

  // Returns a string with the localized shortcut if any.
  base::string16 GetShortcutText() const;

  void set_platform_accelerator(std::unique_ptr<PlatformAccelerator> p) {
    platform_accelerator_ = std::move(p);
  }

  // This class keeps ownership of the returned object.
  const PlatformAccelerator* platform_accelerator() const {
    return platform_accelerator_.get();
  }

  void set_interrupted_by_mouse_event(bool interrupted_by_mouse_event) {
    interrupted_by_mouse_event_ = interrupted_by_mouse_event;
  }

  bool interrupted_by_mouse_event() const {
    return interrupted_by_mouse_event_;
  }

 private:
  // The keycode (VK_...).
  KeyboardCode key_code_;

  KeyState key_state_;

  // The state of the Shift/Ctrl/Alt keys. This corresponds to Event::flags().
  int modifiers_;

  // Stores platform specific data. May be NULL.
  // TODO: this is only used in Mac code and should be removed from here.
  // http://crbug.com/702823.
  std::unique_ptr<PlatformAccelerator> platform_accelerator_;

  // Whether the accelerator is interrupted by a mouse press/release. This is
  // optionally used by AcceleratorController. Even this is set to true, the
  // accelerator may still be handled successfully. (Currently only
  // TOGGLE_APP_LIST is disabled when mouse press/release occurs between
  // search key down and up. See crbug.com/665897)
  bool interrupted_by_mouse_event_;
};

// An interface that classes that want to register for keyboard accelerators
// should implement.
class UI_BASE_EXPORT AcceleratorTarget {
 public:
  // Should return true if the accelerator was processed.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) = 0;

  // Should return true if the target can handle the accelerator events. The
  // AcceleratorPressed method is invoked only for targets for which
  // CanHandleAccelerators returns true.
  virtual bool CanHandleAccelerators() const = 0;

 protected:
  virtual ~AcceleratorTarget() {}
};

// Since accelerator code is one of the few things that can't be cross platform
// in the chrome UI, separate out just the GetAcceleratorForCommandId() from
// the menu delegates.
class AcceleratorProvider {
 public:
  // Gets the accelerator for the specified command id. Returns true if the
  // command id has a valid accelerator, false otherwise.
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          Accelerator* accelerator) const = 0;

 protected:
  virtual ~AcceleratorProvider() {}
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_H_
