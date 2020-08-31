// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

using testing::Return;

class MoveToAccountStoreBubbleControllerTest : public ::testing::Test {
 public:
  MoveToAccountStoreBubbleControllerTest() {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();

    EXPECT_CALL(*delegate(), OnBubbleShown());
    ON_CALL(*delegate(), GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
    controller_ = std::make_unique<MoveToAccountStoreBubbleController>(
        mock_delegate_->AsWeakPtr());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }
  ~MoveToAccountStoreBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  TestingProfile* profile() { return &profile_; }
  MoveToAccountStoreBubbleController* controller() { return controller_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<MoveToAccountStoreBubbleController> controller_;
};

TEST_F(MoveToAccountStoreBubbleControllerTest, CloseExplicitly) {
  EXPECT_CALL(*delegate(), OnBubbleHidden);
  controller()->OnBubbleClosing();
}

TEST_F(MoveToAccountStoreBubbleControllerTest, AcceptMove) {
  EXPECT_CALL(*delegate(), MovePasswordToAccountStore);
  controller()->AcceptMove();
}

TEST_F(MoveToAccountStoreBubbleControllerTest, ProvidesTitle) {
  PasswordBubbleControllerBase* controller_ptr = controller();
  EXPECT_EQ(controller_ptr->GetTitle(),
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_TITLE));
}

TEST_F(MoveToAccountStoreBubbleControllerTest, ProvidesProfileIcon) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager, "todd.tester@gmail.com");
  signin::SimulateAccountImageFetch(
      identity_manager, info.account_id, "https://todd.tester.com/avatar.png",
      gfx::Image(gfx::test::CreateImageSkia(96, 96)));
  EXPECT_FALSE(controller()->GetProfileIcon().IsEmpty());
}

}  // namespace
