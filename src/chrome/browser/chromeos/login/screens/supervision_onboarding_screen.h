// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_ONBOARDING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_ONBOARDING_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class SupervisionOnboardingScreenView;

class SupervisionOnboardingScreen : public BaseScreen {
 public:
  SupervisionOnboardingScreen(SupervisionOnboardingScreenView* view,
                              const base::RepeatingClosure& exit_callback);
  ~SupervisionOnboardingScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(SupervisionOnboardingScreenView* view);

  // Called when supervision onboarding has finished, exits the screen.
  void Exit();

 private:
  SupervisionOnboardingScreenView* view_;
  base::RepeatingClosure exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(SupervisionOnboardingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_ONBOARDING_SCREEN_H_
