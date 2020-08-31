// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_unsynced_credentials_locally_view.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;
using testing::ReturnRef;

class PasswordSaveUnsyncedCredentialsLocallyViewTest
    : public PasswordBubbleViewTestBase {
 public:
  PasswordSaveUnsyncedCredentialsLocallyViewTest() {
    ON_CALL(*model_delegate_mock(), GetUnsyncedCredentials())
        .WillByDefault(ReturnRef(unsynced_credentials_));

    unsynced_credentials_.resize(1);
    unsynced_credentials_[0].username_value = ASCIIToUTF16("user");
    unsynced_credentials_[0].password_value = ASCIIToUTF16("password");
  }
  ~PasswordSaveUnsyncedCredentialsLocallyViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override;

 protected:
  PasswordSaveUnsyncedCredentialsLocallyView* view_;
  std::vector<autofill::PasswordForm> unsynced_credentials_;
};

void PasswordSaveUnsyncedCredentialsLocallyViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new PasswordSaveUnsyncedCredentialsLocallyView(web_contents(),
                                                         anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void PasswordSaveUnsyncedCredentialsLocallyViewTest::TearDown() {
  view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);

  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(PasswordSaveUnsyncedCredentialsLocallyViewTest, HasTitleAndTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view_->ShouldShowWindowTitle());
  EXPECT_TRUE(view_->GetOkButton());
  EXPECT_TRUE(view_->GetCancelButton());
}
