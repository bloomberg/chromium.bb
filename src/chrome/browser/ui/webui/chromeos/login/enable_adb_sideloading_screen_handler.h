// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_ADB_SIDELOADING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_ADB_SIDELOADING_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class EnableAdbSideloadingScreen;
}

namespace chromeos {

// Interface between enable adb sideloading screen and its representation.
class EnableAdbSideloadingScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"adb-sideloading"};

  // The constants need to be synced with oobe_adb_sideloading_screen.js.
  enum class UIState {
    UI_STATE_ERROR = 1,
    UI_STATE_SETUP = 2,
  };

  virtual ~EnableAdbSideloadingScreenView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Bind(ash::EnableAdbSideloadingScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void SetScreenState(UIState value) = 0;
};

// WebUI implementation of EnableAdbSideloadingScreenView.
class EnableAdbSideloadingScreenHandler : public EnableAdbSideloadingScreenView,
                                          public BaseScreenHandler {
 public:
  using TView = EnableAdbSideloadingScreenView;

  explicit EnableAdbSideloadingScreenHandler(
      JSCallsContainer* js_calls_container);

  EnableAdbSideloadingScreenHandler(const EnableAdbSideloadingScreenHandler&) =
      delete;
  EnableAdbSideloadingScreenHandler& operator=(
      const EnableAdbSideloadingScreenHandler&) = delete;

  ~EnableAdbSideloadingScreenHandler() override;

  // EnableAdbSideloadingScreenView implementation:
  void Show() override;
  void Hide() override;
  void Bind(ash::EnableAdbSideloadingScreen* delegate) override;
  void Unbind() override;
  void SetScreenState(UIState value) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  ash::EnableAdbSideloadingScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::EnableAdbSideloadingScreenHandler;
using ::chromeos::EnableAdbSideloadingScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_ADB_SIDELOADING_SCREEN_HANDLER_H_
