// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"

namespace chromeos {

// Mixin browser tests can use for setting up test login manager environment.
// It sets up command line so test starts on the login screen UI, and
// initializes user manager with a list of pre-registered users.
// The mixin will mark the OOBE flow as complete during test setup, so it's not
// suitable for OOBE tests.
class LoginManagerMixin : public InProcessBrowserTestMixin {
 public:
  LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                    const std::vector<AccountId>& initial_users);
  ~LoginManagerMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUpOnMainThread() override;

 private:
  const std::vector<AccountId> initial_users_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_
