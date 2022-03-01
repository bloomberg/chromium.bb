// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"

namespace chromeos {

// Version of AppLaunchSplashScreenHandler used for tests.
class FakeAppLaunchSplashScreenHandler : public AppLaunchSplashScreenView {
 public:
  void SetDelegate(Delegate*) override {}
  void Show() override {}
  void Hide() override {}
  void UpdateAppLaunchState(AppLaunchState state) override;
  void ToggleNetworkConfig(bool) override {}
  void ShowNetworkConfigureUI() override {}
  void ShowErrorMessage(KioskAppLaunchError::Error error) override;
  bool IsNetworkReady() override;

  KioskAppLaunchError::Error GetErrorMessageType() const;
  void SetNetworkReady(bool ready);
  AppLaunchState GetAppLaunchState();

 private:
  KioskAppLaunchError::Error error_message_type_ =
      KioskAppLaunchError::Error::kNone;
  bool network_ready_ = false;
  AppLaunchState state_ = AppLaunchState::kPreparingProfile;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::FakeAppLaunchSplashScreenHandler;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
