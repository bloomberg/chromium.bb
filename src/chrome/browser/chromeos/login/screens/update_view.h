// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_VIEW_H_

#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

class UpdateScreen;

// Interface for dependency injection between WelcomeScreen and its actual
// representation. Owned by UpdateScreen.
class UpdateView {
 public:
  constexpr static OobeScreen kScreenId = OobeScreen::SCREEN_OOBE_UPDATE;

  virtual ~UpdateView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds |screen| to the view.
  virtual void Bind(UpdateScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Set the estimated time left, in seconds.
  virtual void SetEstimatedTimeLeft(int value) = 0;
  virtual void SetShowEstimatedTimeLeft(bool value) = 0;
  virtual void SetUpdateCompleted(bool value) = 0;
  virtual void SetShowCurtain(bool value) = 0;
  virtual void SetProgressMessage(const base::string16& value) = 0;
  virtual void SetProgress(int value) = 0;
  virtual void SetRequiresPermissionForCellular(bool value) = 0;
  virtual void SetCancelUpdateShortcutEnabled(bool value) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_VIEW_H_
