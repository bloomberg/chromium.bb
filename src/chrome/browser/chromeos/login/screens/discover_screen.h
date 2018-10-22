// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DISCOVER_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DISCOVER_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class DiscoverScreenView;

class DiscoverScreen : public BaseScreen {
 public:
  DiscoverScreen(BaseScreenDelegate* base_screen_delegate,
                 DiscoverScreenView* view);
  ~DiscoverScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  DiscoverScreenView* const view_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DISCOVER_SCREEN_H_
