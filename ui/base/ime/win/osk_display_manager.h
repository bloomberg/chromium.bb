// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_OSK_DISPLAY_MANAGER_H_
#define UI_BASE_IME_WIN_OSK_DISPLAY_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/base/ime/ui_base_ime_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class OnScreenKeyboardObserver;

// This class provides functionality to display the on screen keyboard on
// Windows 8+. It optionally notifies observers that the OSK is displayed,
// hidden, etc.
class UI_BASE_IME_EXPORT OnScreenKeyboardDisplayManager {
 public:
  static OnScreenKeyboardDisplayManager* GetInstance();

  virtual ~OnScreenKeyboardDisplayManager();

  // Functions to display and dismiss the keyboard.
  // The optional |observer| parameter allows callers to be notified when the
  // keyboard is displayed, dismissed, etc.
  virtual bool DisplayVirtualKeyboard(OnScreenKeyboardObserver* observer) = 0;
  // When the keyboard is dismissed, the registered observer if any is removed
  // after notifying it.
  virtual bool DismissVirtualKeyboard() = 0;

  // Removes a registered observer.
  virtual void RemoveObserver(OnScreenKeyboardObserver* observer) = 0;

  // Returns true if the virtual keyboard is currently visible.
  virtual bool IsKeyboardVisible() const = 0;

 protected:
  OnScreenKeyboardDisplayManager();
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_OSK_DISPLAY_MANAGER_H_
