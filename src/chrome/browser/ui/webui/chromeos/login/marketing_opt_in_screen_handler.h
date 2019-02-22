// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class MarketingOptInScreen;

// The sole implementation of the MarketingOptInScreenView, using WebUI.
class MarketingOptInScreenHandler : public BaseScreenHandler,
                                    public MarketingOptInScreenView {
 public:
  MarketingOptInScreenHandler();
  ~MarketingOptInScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // MarketingOptInScreenView:
  void Bind(MarketingOptInScreen* screen) override;
  void Show() override;
  void Hide() override;

 private:
  // BaseScreenHandler:
  void Initialize() override;
  void RegisterMessages() override;

  // WebUI event handler.
  void HandleAllSet(bool play_communications_opt_in,
                    bool tips_communications_opt_in);

  MarketingOptInScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MarketingOptInScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_
