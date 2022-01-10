// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/ax_event_counter.h"

using base::StartsWith;

class PasswordBubbleBrowserTest
    : public SupportsTestDialog<ManagePasswordsTest> {
 public:
  PasswordBubbleBrowserTest() = default;
  ~PasswordBubbleBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    if (StartsWith(name, "PendingPasswordBubble",
                   base::CompareCase::SENSITIVE)) {
      SetupPendingPassword();
    } else if (StartsWith(name, "AutomaticPasswordBubble",
                          base::CompareCase::SENSITIVE)) {
      SetupAutomaticPassword();
    } else if (StartsWith(name, "ManagePasswordBubble",
                          base::CompareCase::SENSITIVE)) {
      // Set test form to be account-stored. Otherwise, there is no indicator.
      test_form()->in_store =
          password_manager::PasswordForm::Store::kAccountStore;
      SetupManagingPasswords();
      ExecuteManagePasswordsCommand();
    } else if (StartsWith(name, "AutoSignin", base::CompareCase::SENSITIVE)) {
      test_form()->url = GURL("https://example.com");
      test_form()->display_name = u"Peter";
      test_form()->username_value = u"pet12@gmail.com";
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials;
      local_credentials.push_back(
          std::make_unique<password_manager::PasswordForm>(*test_form()));

      PasswordAutoSignInView::set_auto_signin_toast_timeout(10);
      SetupAutoSignin(std::move(local_credentials));
    } else if (StartsWith(name, "MoveToAccountStoreBubble",
                          base::CompareCase::SENSITIVE)) {
      SetupMovingPasswords();
    } else if (StartsWith(name, "SafeState", base::CompareCase::SENSITIVE)) {
      SetupSafeState();
    } else if (StartsWith(name, "MoreToFixState",
                          base::CompareCase::SENSITIVE)) {
      SetupMoreToFixState();
    } else {
      ADD_FAILURE() << "Unknown dialog type";
      return;
    }
  }
};

IN_PROC_BROWSER_TEST_F(PasswordBubbleBrowserTest,
                       InvokeUi_PendingPasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleBrowserTest,
                       InvokeUi_AutomaticPasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleBrowserTest,
                       InvokeUi_ManagePasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleBrowserTest, InvokeUi_AutoSignin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleBrowserTest, InvokeUi_SafeState) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleBrowserTest, InvokeUi_MoreToFixState) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleBrowserTest,
                       InvokeUi_MoveToAccountStoreBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleBrowserTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  // This needs to show a password bubble that does not trigger as a user
  // gesture in order to fire an alert event. See
  // LocationBarBubbleDelegateView's calls to SetAccessibleRole().
  ShowUi("AutomaticPasswordBubble");
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}
