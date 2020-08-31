// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "components/login/base_screen_handler_utils.h"

namespace chromeos {

// Base class for the all OOBE/login/before-session screens.
// Screens are identified by ID, screen and it's JS counterpart must have same
// id.
// Most of the screens will be re-created for each appearance with Initialize()
// method called just once.
class BaseScreen {
 public:
  // String which represents not applicable exit code. This exit code refers to
  // skipping the screen due to specific unmet condition.
  constexpr static const char kNotApplicable[] = "NotApplicable";

  BaseScreen(OobeScreenId screen_id, OobeScreenPriority screen_priority);
  virtual ~BaseScreen();

  // Makes wizard screen visible.
  void Show();

  // Makes wizard screen invisible.
  void Hide();

  // Returns whether the screen should be skipped i. e. should be exited due to
  // specific unmet conditions. Returns true if skips the screen.
  virtual bool MaybeSkip() WARN_UNUSED_RESULT;

  // Forwards user action if screen is shown.
  void HandleUserAction(const std::string& action_id);

  // Returns the identifier of the screen.
  OobeScreenId screen_id() const { return screen_id_; }

  // Returns the priority of the screen.
  OobeScreenPriority screen_priority() const { return screen_priority_; }

  // Change the configuration for the screen. |configuration| is unowned.
  virtual void SetConfiguration(base::Value* configuration);

  bool is_hidden() { return is_hidden_; }

 protected:
  virtual void ShowImpl() = 0;
  virtual void HideImpl() = 0;

  // Called when user action event with |event_id|
  // happened. Notification about this event comes from the JS
  // counterpart. Not called if the screen is hidden
  virtual void OnUserAction(const std::string& action_id);

  // Global configuration for OOBE screens, that can be used to automate some
  // screens.
  // Screens can use values in Configuration to fill in UI values or
  // automatically finish.
  // Configuration is guaranteed to exist between pair of OnShow/OnHide calls,
  // no external changes will be made to configuration during that time.
  // Outside that time the configuration is set to nullptr to prevent any logic
  // triggering while the screen is not displayed.
  base::Value* GetConfiguration() { return configuration_; }

 private:
  bool is_hidden_ = true;

  // Configuration itself is owned by WizardController and is accessible
  // to screen only between OnShow / OnHide calls.
  base::Value* configuration_ = nullptr;

  const OobeScreenId screen_id_;

  const OobeScreenPriority screen_priority_;

  DISALLOW_COPY_AND_ASSIGN(BaseScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
