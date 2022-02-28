// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class MultiDeviceSetupScreen;
}

namespace chromeos {

// Interface for dependency injection between MultiDeviceSetupScreen and its
// WebUI representation.
class MultiDeviceSetupScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"multidevice-setup-screen"};

  virtual ~MultiDeviceSetupScreenView() = default;

  virtual void Bind(ash::MultiDeviceSetupScreen* screen) = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
};

// Concrete MultiDeviceSetupScreenView WebUI-based implementation.
class MultiDeviceSetupScreenHandler : public BaseScreenHandler,
                                      public MultiDeviceSetupScreenView {
 public:
  using TView = MultiDeviceSetupScreenView;

  explicit MultiDeviceSetupScreenHandler(JSCallsContainer* js_calls_container);

  MultiDeviceSetupScreenHandler(const MultiDeviceSetupScreenHandler&) = delete;
  MultiDeviceSetupScreenHandler& operator=(
      const MultiDeviceSetupScreenHandler&) = delete;

  ~MultiDeviceSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;

  // MultiDeviceSetupScreenView:
  void Bind(ash::MultiDeviceSetupScreen* screen) override;
  void Show() override;
  void Hide() override;

 private:
  // BaseScreenHandler:
  void Initialize() override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::MultiDeviceSetupScreenHandler;
using ::chromeos::MultiDeviceSetupScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_
