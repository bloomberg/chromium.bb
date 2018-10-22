// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WELCOME_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WELCOME_VIEW_H_

#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

class WelcomeScreen;

// Interface for dependency injection between WelcomeScreen and its actual
// representation, either views based or WebUI. Owned by WelcomeScreen.
class WelcomeView {
 public:
  constexpr static OobeScreen kScreenId = OobeScreen::SCREEN_OOBE_WELCOME;

  virtual ~WelcomeView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds |screen| to the view.
  virtual void Bind(WelcomeScreen* screen) = 0;

  // Unbinds model from the view.
  virtual void Unbind() = 0;

  // Stops demo mode detection.
  virtual void StopDemoModeDetection() = 0;

  // Reloads localized contents.
  virtual void ReloadLocalizedContent() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WELCOME_VIEW_H_
