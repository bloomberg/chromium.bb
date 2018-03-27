// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_STUB_H_
#define UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_STUB_H_

#include "ui/base/ime/ui_base_ime_export.h"
#include "ui/base/ime/win/osk_display_manager.h"

namespace ui {

// This class provides a stub OnScreenDisplayManager.
// Used for < Win8 OS versions.
class UI_BASE_IME_EXPORT OnScreenKeyboardDisplayManagerStub
    : public OnScreenKeyboardDisplayManager {
 public:
  ~OnScreenKeyboardDisplayManagerStub() override;

  // OnScreenKeyboardDisplayManager overrides.
  bool DisplayVirtualKeyboard(OnScreenKeyboardObserver* observer) final;
  bool DismissVirtualKeyboard() final;
  void RemoveObserver(OnScreenKeyboardObserver* observer) final;
  bool IsKeyboardVisible() const final;

 private:
  friend class OnScreenKeyboardDisplayManager;
  OnScreenKeyboardDisplayManagerStub();

  DISALLOW_COPY_AND_ASSIGN(OnScreenKeyboardDisplayManagerStub);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_STUB_H_
