// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/strings/string16.h"

namespace aura {
class Window;
}

namespace ws {
class InputDeviceControllerClient;
}

namespace ash {

class AccessibilityDelegate;
class ScreenshotDelegate;

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Returns true if |window| can be shown for the delegate's concept of current
  // user.
  virtual bool CanShowWindowForUser(const aura::Window* window) const = 0;

  // TODO(jamescook): Replace with a mojo-compatible interface.
  virtual std::unique_ptr<ScreenshotDelegate> CreateScreenshotDelegate() = 0;

  // Creates a accessibility delegate. Shell takes ownership of the delegate.
  virtual AccessibilityDelegate* CreateAccessibilityDelegate() = 0;

  virtual void OpenKeyboardShortcutHelpPage() const {}

  // Creator of Shell owns this; it's assumed this outlives Shell.
  virtual ws::InputDeviceControllerClient* GetInputDeviceControllerClient() = 0;
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
