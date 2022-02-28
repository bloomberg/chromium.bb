// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_QUICK_START_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_QUICK_START_SCREEN_HANDLER_H_

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class QuickStartScreen;
}

namespace login {
class LocalizedValuesBuilder;
}

namespace chromeos {

class JSCallsContainer;

class QuickStartView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"quick-start"};

  virtual ~QuickStartView() = default;

  virtual void Show() = 0;
  virtual void Bind(ash::QuickStartScreen* screen) = 0;
  virtual void Unbind() = 0;
};

// WebUI implementation of QuickStartView.
class QuickStartScreenHandler : public QuickStartView,
                                public BaseScreenHandler {
 public:
  using TView = QuickStartView;

  explicit QuickStartScreenHandler(JSCallsContainer* js_calls_container);

  QuickStartScreenHandler(const QuickStartScreenHandler&) = delete;
  QuickStartScreenHandler& operator=(const QuickStartScreenHandler&) = delete;

  ~QuickStartScreenHandler() override;

  // QuickStartView:
  void Show() override;
  void Bind(ash::QuickStartScreen* screen) override;
  void Unbind() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  ash::QuickStartScreen* screen_ = nullptr;

  // If true, Initialize() will call Show().
  bool show_on_init_ = false;
};

}  // namespace chromeos

namespace ash {
using chromeos::QuickStartScreenHandler;
using chromeos::QuickStartView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_QUICK_START_SCREEN_HANDLER_H_
