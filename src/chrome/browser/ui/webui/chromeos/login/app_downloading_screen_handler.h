// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class AppDownloadingScreenView
    : public base::SupportsWeakPtr<AppDownloadingScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"app-downloading",
                                                       "AppDownloadingScreen"};

  virtual ~AppDownloadingScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;
};

// The sole implementation of the AppDownloadingScreenView, using WebUI.
class AppDownloadingScreenHandler : public BaseScreenHandler,
                                    public AppDownloadingScreenView {
 public:
  using TView = AppDownloadingScreenView;

  AppDownloadingScreenHandler();

  AppDownloadingScreenHandler(const AppDownloadingScreenHandler&) = delete;
  AppDownloadingScreenHandler& operator=(const AppDownloadingScreenHandler&) =
      delete;

  ~AppDownloadingScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // AppDownloadingScreenView:
  void Show() final;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::AppDownloadingScreenHandler;
using ::chromeos::AppDownloadingScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_
