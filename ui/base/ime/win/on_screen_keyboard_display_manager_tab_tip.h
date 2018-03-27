// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAB_TIP_H_
#define UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAB_TIP_H_

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "ui/base/ime/ui_base_ime_export.h"
#include "ui/base/ime/win/osk_display_manager.h"

namespace ui {

class OnScreenKeyboardDetector;

// This class provides an implementation of the OnScreenKeyboardDisplayManager
// that uses heuristics and the TabTip.exe to display the on screen keyboard.
// Used on Windows > 7 and Windows < 10.0.10240.0
class UI_BASE_IME_EXPORT OnScreenKeyboardDisplayManagerTabTip
    : public OnScreenKeyboardDisplayManager {
 public:
  ~OnScreenKeyboardDisplayManagerTabTip() override;

  // OnScreenKeyboardDisplayManager overrides.
  bool DisplayVirtualKeyboard(OnScreenKeyboardObserver* observer) final;
  bool DismissVirtualKeyboard() final;
  void RemoveObserver(OnScreenKeyboardObserver* observer) final;
  bool IsKeyboardVisible() const final;

  // Returns the path of the on screen keyboard exe (TabTip.exe) in the
  // |osk_path| parameter.
  // Returns true on success.
  bool GetOSKPath(base::string16* osk_path);

 private:
  friend class OnScreenKeyboardDisplayManager;
  friend class OnScreenKeyboardTest;
  OnScreenKeyboardDisplayManagerTabTip();

  std::unique_ptr<OnScreenKeyboardDetector> keyboard_detector_;

  // The location of TabTip.exe.
  base::string16 osk_path_;

  DISALLOW_COPY_AND_ASSIGN(OnScreenKeyboardDisplayManagerTabTip);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAB_TIP_H_
