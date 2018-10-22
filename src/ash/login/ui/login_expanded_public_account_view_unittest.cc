// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/public_account_warning_dialog.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Total width of the expanded view.
constexpr int kBubbleTotalWidthDp = 600;
// Total height of the expanded view.
constexpr int kBubbleTotalHeightDp = 324;

class LoginExpandedPublicAccountViewTest : public LoginTestBase {
 protected:
  LoginExpandedPublicAccountViewTest() = default;
  ~LoginExpandedPublicAccountViewTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    user_ = CreatePublicAccountUser("user@domain.com");
    public_account_ = new LoginExpandedPublicAccountView(base::DoNothing());
    public_account_->UpdateForUser(user_);

    other_view_ = new views::View();

    container_ = new views::View();
    container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
    container_->AddChildView(public_account_);
    container_->AddChildView(other_view_);
    SetWidget(CreateWidgetWithContent(container_));
  }

  mojom::LoginUserInfoPtr user_;

  // Owned by test widget view hierarchy.
  views::View* container_ = nullptr;
  LoginExpandedPublicAccountView* public_account_ = nullptr;
  views::View* other_view_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginExpandedPublicAccountViewTest);
};

}  // namespace

// Verifies toggle advanced view will update the layout correctly.
TEST_F(LoginExpandedPublicAccountViewTest, ToggleAdvancedView) {
  public_account_->SizeToPreferredSize();
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);

  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  EXPECT_FALSE(user_->public_account_info->show_advanced_view);
  EXPECT_FALSE(test_api.advanced_view()->visible());

  // Toggle show_advanced_view.
  user_->public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);

  // Advanced view is shown and the overall size does not change.
  EXPECT_TRUE(test_api.advanced_view()->visible());
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);

  // Click on the show advanced button.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      test_api.advanced_view_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // Advanced view is hidden and the overall size does not change.
  EXPECT_FALSE(test_api.advanced_view()->visible());
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);
}

// Verifies warning dialog shows up correctly.
TEST_F(LoginExpandedPublicAccountViewTest, ShowWarningDialog) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  views::StyledLabel::TestApi styled_label_test(test_api.learn_more_label());
  EXPECT_EQ(test_api.warning_dialog(), nullptr);
  EXPECT_EQ(styled_label_test.link_targets().size(), 1U);

  // Tap on the learn more link.
  views::View* link_view = styled_label_test.link_targets().begin()->first;
  GetEventGenerator()->MoveMouseTo(
      link_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_NE(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(test_api.warning_dialog()->IsVisible());

  // When warning dialog is shown, tap outside of public account expanded view
  // should not hide it.
  GetEventGenerator()->MoveMouseTo(
      other_view_->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(public_account_->visible());
  EXPECT_NE(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(test_api.warning_dialog()->IsVisible());

  // If the warning dialog is shown, escape key should close the waring dialog,
  // but not the public account view.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(public_account_->visible());

  // Press escape again should hide the public account expanded view.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  EXPECT_FALSE(public_account_->visible());
}

}  // namespace ash
