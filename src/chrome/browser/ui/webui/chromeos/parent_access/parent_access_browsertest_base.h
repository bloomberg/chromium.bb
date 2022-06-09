// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_BROWSERTEST_BASE_H_

#include <memory>

#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.h"
#include "content/public/browser/web_contents.h"

namespace signin {
class IdentityTestEnvironment;
}

namespace chromeos {

// Base class for ParentAccessUI tests.
class ParentAccessBrowserTestBase : public MixinBasedInProcessBrowserTest {
 public:
  ParentAccessBrowserTestBase();

  ParentAccessBrowserTestBase(const ParentAccessBrowserTestBase&) = delete;
  ParentAccessBrowserTestBase& operator=(const ParentAccessBrowserTestBase&) =
      delete;

  ~ParentAccessBrowserTestBase() override;

  void SetUp() override;
  void SetUpOnMainThread() override;

  chromeos::ParentAccessUI* GetParentAccessUI();
  content::WebContents* contents();

 protected:
  virtual LoggedInUserMixin::LogInType GetLogInType() = 0;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;

 private:
  std::unique_ptr<LoggedInUserMixin> logged_in_user_mixin_;
};

// Base class for ParentAccessUI tests using a regular user.
class ParentAccessRegularUserBrowserTestBase
    : public ParentAccessBrowserTestBase {
 public:
  ParentAccessRegularUserBrowserTestBase();

  ParentAccessRegularUserBrowserTestBase(
      const ParentAccessRegularUserBrowserTestBase&) = delete;
  ParentAccessRegularUserBrowserTestBase& operator=(
      const ParentAccessRegularUserBrowserTestBase&) = delete;

  ~ParentAccessRegularUserBrowserTestBase() override;

 protected:
  LoggedInUserMixin::LogInType GetLogInType() override;
};

// Base class for ParentAccessUI tests using a child user.
class ParentAccessChildUserBrowserTestBase
    : public ParentAccessBrowserTestBase {
 public:
  ParentAccessChildUserBrowserTestBase();

  ParentAccessChildUserBrowserTestBase(
      const ParentAccessChildUserBrowserTestBase&) = delete;
  ParentAccessChildUserBrowserTestBase& operator=(
      const ParentAccessChildUserBrowserTestBase&) = delete;

  ~ParentAccessChildUserBrowserTestBase() override;

 protected:
  LoggedInUserMixin::LogInType GetLogInType() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_BROWSERTEST_BASE_H_
