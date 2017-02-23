// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_client_view.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

// Base class for tests. Also acts as the dialog delegate and contents view for
// TestDialogClientView.
class DialogClientViewTest : public test::WidgetTest,
                             public DialogDelegateView {
 public:
  DialogClientViewTest() {}

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();

    // Note: not using DialogDelegate::CreateDialogWidget(..), since that can
    // alter the frame type according to the platform.
    widget_ = new views::Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.delegate = this;
    widget_->Init(params);
    EXPECT_EQ(this, GetContentsView());
  }

  void TearDown() override {
    widget_->CloseNow();
    WidgetTest::TearDown();
  }

  // DialogDelegateView:
  ClientView* CreateClientView(Widget* widget) override {
    client_view_ = new DialogClientView(widget, this);
    return client_view_;
  }

  bool ShouldUseCustomFrame() const override { return false; }

  void DeleteDelegate() override {
    // DialogDelegateView would delete this, but |this| is owned by the test.
  }

  View* CreateExtraView() override { return next_extra_view_.release(); }

  bool GetExtraViewPadding(int* padding) override {
    if (extra_view_padding_)
      *padding = *extra_view_padding_;
    return extra_view_padding_.get() != nullptr;
  }

  int GetDialogButtons() const override { return dialog_buttons_; }

 protected:
  gfx::Rect GetUpdatedClientBounds() {
    client_view_->SizeToPreferredSize();
    client_view_->Layout();
    return client_view_->bounds();
  }

  // Makes sure that the content view is sized correctly. Width must be at least
  // the requested amount, but height should always match exactly.
  void CheckContentsIsSetToPreferredSize() {
    const gfx::Rect client_bounds = GetUpdatedClientBounds();
    const gfx::Size preferred_size = this->GetPreferredSize();
    EXPECT_EQ(preferred_size.height(), this->bounds().height());
    EXPECT_LE(preferred_size.width(), this->bounds().width());
    EXPECT_EQ(gfx::Point(), this->origin());
    EXPECT_EQ(client_bounds.width(), this->width());
  }

  // Sets the buttons to show in the dialog and refreshes the dialog.
  void SetDialogButtons(int dialog_buttons) {
    dialog_buttons_ = dialog_buttons;
    client_view_->UpdateDialogButtons();
  }

  // Sets the view to provide to CreateExtraView() and updates the dialog. This
  // can only be called a single time because DialogClientView caches the result
  // of CreateExtraView() and never calls it again.
  void SetExtraView(View* view) {
    EXPECT_FALSE(next_extra_view_);
    next_extra_view_ = base::WrapUnique(view);
    client_view_->UpdateDialogButtons();
    EXPECT_FALSE(next_extra_view_);
  }

  // Sets the extra view padding.
  void SetExtraViewPadding(int padding) {
    DCHECK(!extra_view_padding_);
    extra_view_padding_.reset(new int(padding));
    client_view_->Layout();
  }

  View* FocusableViewAfter(View* view) {
    const bool dont_loop = false;
    const bool reverse = false;
    return GetFocusManager()->GetNextFocusableView(view, GetWidget(), reverse,
                                                   dont_loop);
  }

  DialogClientView* client_view() { return client_view_; }

 private:
  // The dialog Widget.
  Widget* widget_ = nullptr;

  // The DialogClientView that's being tested. Owned by |widget_|.
  DialogClientView* client_view_;

  // The bitmask of buttons to show in the dialog.
  int dialog_buttons_ = ui::DIALOG_BUTTON_NONE;

  // Set and cleared in SetExtraView().
  std::unique_ptr<View> next_extra_view_;

  std::unique_ptr<int> extra_view_padding_;

  DISALLOW_COPY_AND_ASSIGN(DialogClientViewTest);
};

TEST_F(DialogClientViewTest, UpdateButtons) {
  // This dialog should start with no buttons.
  EXPECT_EQ(GetDialogButtons(), ui::DIALOG_BUTTON_NONE);
  EXPECT_EQ(NULL, client_view()->ok_button());
  EXPECT_EQ(NULL, client_view()->cancel_button());
  const int height_without_buttons = GetUpdatedClientBounds().height();

  // Update to use both buttons.
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  EXPECT_TRUE(client_view()->ok_button()->is_default());
  EXPECT_FALSE(client_view()->cancel_button()->is_default());
  const int height_with_buttons = GetUpdatedClientBounds().height();
  EXPECT_GT(height_with_buttons, height_without_buttons);

  // Remove the dialog buttons.
  SetDialogButtons(ui::DIALOG_BUTTON_NONE);
  EXPECT_EQ(NULL, client_view()->ok_button());
  EXPECT_EQ(NULL, client_view()->cancel_button());
  EXPECT_EQ(GetUpdatedClientBounds().height(), height_without_buttons);

  // Reset with just an ok button.
  SetDialogButtons(ui::DIALOG_BUTTON_OK);
  EXPECT_TRUE(client_view()->ok_button()->is_default());
  EXPECT_EQ(NULL, client_view()->cancel_button());
  EXPECT_EQ(GetUpdatedClientBounds().height(), height_with_buttons);

  // Reset with just a cancel button.
  SetDialogButtons(ui::DIALOG_BUTTON_CANCEL);
  EXPECT_EQ(NULL, client_view()->ok_button());
  EXPECT_EQ(client_view()->cancel_button()->is_default(),
            PlatformStyle::kDialogDefaultButtonCanBeCancel);
  EXPECT_EQ(GetUpdatedClientBounds().height(), height_with_buttons);
}

TEST_F(DialogClientViewTest, RemoveAndUpdateButtons) {
  // Removing buttons from another context should clear the local pointer.
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  delete client_view()->ok_button();
  EXPECT_EQ(NULL, client_view()->ok_button());
  delete client_view()->cancel_button();
  EXPECT_EQ(NULL, client_view()->cancel_button());

  // Updating should restore the requested buttons properly.
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  EXPECT_TRUE(client_view()->ok_button()->is_default());
  EXPECT_FALSE(client_view()->cancel_button()->is_default());
}

// Test that views inside the dialog client view have the correct focus order.
TEST_F(DialogClientViewTest, SetupFocusChain) {
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  const bool kIsOkButtonOnLeftSide = true;
#else
  const bool kIsOkButtonOnLeftSide = false;
#endif

  // Initially the dialog client view only contains the content view.
  EXPECT_EQ(nullptr, GetContentsView()->GetNextFocusableView());

  // Add OK and cancel buttons.
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);

  if (kIsOkButtonOnLeftSide) {
    EXPECT_EQ(client_view()->ok_button(),
              GetContentsView()->GetNextFocusableView());
    EXPECT_EQ(client_view()->cancel_button(),
              client_view()->ok_button()->GetNextFocusableView());
    EXPECT_EQ(nullptr, client_view()->cancel_button()->GetNextFocusableView());
  } else {
    EXPECT_EQ(client_view()->cancel_button(),
              GetContentsView()->GetNextFocusableView());
    EXPECT_EQ(client_view()->ok_button(),
              client_view()->cancel_button()->GetNextFocusableView());
    EXPECT_EQ(nullptr, client_view()->ok_button()->GetNextFocusableView());
  }

  // Add extra view and remove OK button.
  View* extra_view = new StaticSizedView(gfx::Size(200, 200));
  extra_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  SetExtraView(extra_view);
  SetDialogButtons(ui::DIALOG_BUTTON_CANCEL);

  EXPECT_EQ(extra_view, GetContentsView()->GetNextFocusableView());
  EXPECT_EQ(client_view()->cancel_button(), extra_view->GetNextFocusableView());
  EXPECT_EQ(nullptr, client_view()->cancel_button()->GetNextFocusableView());

  // Add a dummy view to the contents view. Consult the FocusManager for the
  // traversal order since it now spans different levels of the view hierarchy.
  View* dummy_view = new StaticSizedView(gfx::Size(200, 200));
  dummy_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  GetContentsView()->AddChildView(dummy_view);
  EXPECT_EQ(dummy_view, FocusableViewAfter(client_view()->cancel_button()));
  EXPECT_EQ(extra_view, FocusableViewAfter(dummy_view));
  EXPECT_EQ(client_view()->cancel_button(), FocusableViewAfter(extra_view));

  // Views are added to the contents view, not the client view, so the focus
  // chain within the client view is not affected.
  EXPECT_EQ(nullptr, client_view()->cancel_button()->GetNextFocusableView());
}

// Test that the contents view gets its preferred size in the basic dialog
// configuration.
TEST_F(DialogClientViewTest, ContentsSize) {
  CheckContentsIsSetToPreferredSize();
  EXPECT_EQ(GetContentsView()->size(), client_view()->size());
  // There's nothing in the contents view (i.e. |this|), so it should be 0x0.
  EXPECT_EQ(gfx::Size(), client_view()->size());
}

// Test the effect of the button strip on layout.
TEST_F(DialogClientViewTest, LayoutWithButtons) {
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  CheckContentsIsSetToPreferredSize();

  EXPECT_LT(GetContentsView()->bounds().bottom(),
            client_view()->bounds().bottom());
  gfx::Size no_extra_view_size = client_view()->bounds().size();

  View* extra_view = new StaticSizedView(gfx::Size(200, 200));
  SetExtraView(extra_view);
  CheckContentsIsSetToPreferredSize();
  EXPECT_GT(client_view()->bounds().height(), no_extra_view_size.height());
  int width_of_dialog = client_view()->bounds().width();
  int width_of_extra_view = extra_view->bounds().width();

  // Try with an adjusted padding for the extra view.
  SetExtraViewPadding(250);
  CheckContentsIsSetToPreferredSize();
  EXPECT_GT(client_view()->bounds().width(), width_of_dialog);

  // Visibility of extra view is respected.
  extra_view->SetVisible(false);
  CheckContentsIsSetToPreferredSize();
  EXPECT_EQ(no_extra_view_size.height(), client_view()->bounds().height());
  EXPECT_EQ(no_extra_view_size.width(), client_view()->bounds().width());

  // Try with a reduced-size dialog.
  extra_view->SetVisible(true);
  client_view()->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), no_extra_view_size));
  client_view()->Layout();
  EXPECT_GT(width_of_extra_view, extra_view->bounds().width());
}

}  // namespace views
