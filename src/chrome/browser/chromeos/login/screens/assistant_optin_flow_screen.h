// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class AssistantOptInFlowScreenView;
class BaseScreenDelegate;

class AssistantOptInFlowScreen : public BaseScreen {
 public:
  AssistantOptInFlowScreen(BaseScreenDelegate* base_screen_delegate,
                           AssistantOptInFlowScreenView* view);
  ~AssistantOptInFlowScreen() override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(AssistantOptInFlowScreenView* view_);

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  AssistantOptInFlowScreenView* view_;

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInFlowScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_
