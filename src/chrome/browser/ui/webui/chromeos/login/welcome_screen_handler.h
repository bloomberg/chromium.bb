// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/welcome_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace base {
class ListValue;
}

namespace chromeos {

class CoreOobeView;

// WebUI implementation of WelcomeScreenView. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class WelcomeScreenHandler : public WelcomeView, public BaseScreenHandler {
 public:
  explicit WelcomeScreenHandler(CoreOobeView* core_oobe_view);
  ~WelcomeScreenHandler() override;

 private:
  // WelcomeView implementation:
  void Show() override;
  void Hide() override;
  void Bind(WelcomeScreen* screen) override;
  void Unbind() override;
  void StopDemoModeDetection() override;
  void ReloadLocalizedContent() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void Initialize() override;

  // Returns available timezones. Caller gets the ownership.
  static std::unique_ptr<base::ListValue> GetTimezoneList();

  CoreOobeView* core_oobe_view_ = nullptr;
  WelcomeScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(WelcomeScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_
