// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_VIEW_H_

#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

class AssistantOptInFlowScreen;

// Interface for dependency injection between AssistantOptInFlowScreen
// and its WebUI representation.
class AssistantOptInFlowScreenView {
 public:
  constexpr static OobeScreen kScreenId =
      OobeScreen::SCREEN_ASSISTANT_OPTIN_FLOW;

  virtual ~AssistantOptInFlowScreenView() = default;

  virtual void Bind(AssistantOptInFlowScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  AssistantOptInFlowScreenView() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantOptInFlowScreenView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_VIEW_H_
