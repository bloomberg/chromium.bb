// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/sign_in_promo_bubble_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace {

MATCHER_P(AccountEq, expected, "") {
  return expected.account_id == arg.account_id && expected.email == arg.email &&
         expected.gaia == arg.gaia;
}

}  // namespace

class SignInPromoBubbleControllerTest : public ::testing::Test {
 public:
  SignInPromoBubbleControllerTest() {
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }
  ~SignInPromoBubbleControllerTest() override = default;

  std::vector<std::unique_ptr<autofill::PasswordForm>> GetCurrentForms() const;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  SignInPromoBubbleController* controller() { return controller_.get(); }
  TestingProfile* profile() { return &profile_; }

  void Init();
  void DestroyController();

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<SignInPromoBubbleController> controller_;
};

void SignInPromoBubbleControllerTest::Init() {
  EXPECT_CALL(*delegate(), GetWebContents())
      .WillRepeatedly(Return(test_web_contents_.get()));

  controller_.reset(
      new SignInPromoBubbleController(mock_delegate_->AsWeakPtr()));
}

#if !defined(OS_CHROMEOS)
TEST_F(SignInPromoBubbleControllerTest, SignInPromoOK) {
  Init();
  AccountInfo account;
  account.account_id = CoreAccountId("foo_account_id");
  account.gaia = "foo_gaia_id";
  account.email = "foo@bar.com";
  EXPECT_CALL(*delegate(), EnableSync(AccountEq(account), false));

  controller()->OnSignInToChromeClicked(account,
                                        false /* is_default_promo_account */);
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked));
}
#endif
