// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_CONTROLLER_OBSERVER_H_
#define UI_KEYBOARD_KEYBOARD_CONTROLLER_OBSERVER_H_

#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_export.h"

namespace gfx {
class Rect;
}

namespace keyboard {

// Observers to the KeyboardController are notified of significant events that
// occur with the keyboard, such as the bounds or visiility changing.
class KEYBOARD_EXPORT KeyboardControllerObserver {
 public:
  virtual ~KeyboardControllerObserver() {}

  // DEPRECATED
  // Called when the keyboard bounds or visibility are about to change.
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) = 0;

  // Called when the keyboard is shown or closed.
  virtual void OnKeyboardAvailabilityChanging(const bool is_available) {}

  // Called when the keyboard bounds are changing.
  virtual void OnKeyboardVisibleBoundsChanging(const gfx::Rect& new_bounds) {}

  // Called when the keyboard bounds have changed in a way that should affect
  // the usable region of the workspace.
  virtual void OnKeyboardWorkspaceOccludedBoundsChanging(
      const gfx::Rect& new_bounds) {}

  // Called when the keyboard was closed.
  virtual void OnKeyboardClosed() = 0;

  // Called when the keyboard has been hidden and the hiding animation finished
  // successfully. This is same as |state| == HIDDEN on OnStateChanged.
  virtual void OnKeyboardHidden() {}

  // Called when the state changed.
  virtual void OnStateChanged(const KeyboardControllerState state) {}

  // Called when the virtual keyboard IME config changed.
  virtual void OnKeyboardConfigChanged() {}
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_CONTROLLER_OBSERVER_H_
