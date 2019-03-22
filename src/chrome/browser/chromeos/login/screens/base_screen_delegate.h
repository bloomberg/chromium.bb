// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_DELEGATE_H_

#include "chrome/browser/chromeos/login/screens/screen_exit_code.h"

namespace chromeos {

class BaseScreen;
class ErrorScreen;

// Interface that handles notifications received from any of login wizard
// screens.
class BaseScreenDelegate {
 public:
  // Method called by a screen when user's done with it.
  virtual void OnExit(ScreenExitCode exit_code) = 0;

  // Forces current screen showing.
  virtual void ShowCurrentScreen() = 0;

  virtual ErrorScreen* GetErrorScreen() = 0;
  virtual void ShowErrorScreen() = 0;
  virtual void HideErrorScreen(BaseScreen* parent_screen) = 0;

 protected:
  virtual ~BaseScreenDelegate() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_DELEGATE_H_
