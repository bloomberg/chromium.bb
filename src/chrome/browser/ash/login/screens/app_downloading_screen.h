// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"

namespace ash {

// This is App Downloading screen that tells the user the selected Android apps
// are being downloaded.
class AppDownloadingScreen : public BaseScreen {
 public:
  using TView = AppDownloadingScreenView;

  AppDownloadingScreen(AppDownloadingScreenView* view,
                       const base::RepeatingClosure& exit_callback);

  AppDownloadingScreen(const AppDownloadingScreen&) = delete;
  AppDownloadingScreen& operator=(const AppDownloadingScreen&) = delete;

  ~AppDownloadingScreen() override;

  void set_exit_callback_for_testing(base::RepeatingClosure exit_callback) {
    exit_callback_ = exit_callback;
  }

  // This method is called, when view is being destroyed. Note, if model
  // is destroyed earlier then it has to call SetModel(NULL).
  void OnViewDestroyed(AppDownloadingScreenView* view);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  AppDownloadingScreenView* view_;
  base::RepeatingClosure exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// migration is finished.
namespace chromeos {
using ::ash::AppDownloadingScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_
