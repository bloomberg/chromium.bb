// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

namespace {

class TestDialog : public DialogDelegateView, public ButtonListener {
 public:
  TestDialog()
      : canceled_(false),
        accepted_(false),
        closeable_(false),
        last_pressed_button_(NULL) {}
  ~TestDialog() override {}

  // WidgetDelegate overrides:
  bool ShouldShowWindowTitle() const override {
    return !title_.empty();
  }

  // DialogDelegateView overrides:
  bool Cancel() override {
    canceled_ = true;
    return closeable_;
  }
  bool Accept() override {
    accepted_ = true;
    return closeable_;
  }

  // DialogDelegateView overrides:
  gfx::Size GetPreferredSize() const override { return gfx::Size(200, 200); }
  base::string16 GetWindowTitle() const override { return title_; }
  bool UseNewStyleForThisDialog() const override { return true; }

  // ButtonListener override:
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    last_pressed_button_ = sender;
  }

  Button* last_pressed_button() const { return last_pressed_button_; }

  void PressEnterAndCheckStates(Button* button) {
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
    GetFocusManager()->OnKeyEvent(key_event);
    const DialogClientView* client_view = GetDialogClientView();
    EXPECT_EQ(canceled_, client_view->cancel_button()->is_default());
    EXPECT_EQ(accepted_, client_view->ok_button()->is_default());
    // This view does not listen for ok or cancel clicks, DialogClientView does.
    CheckAndResetStates(button == client_view->cancel_button(),
                        button == client_view->ok_button(),
                        (canceled_ || accepted_ ) ? NULL : button);
  }

  void CheckAndResetStates(bool canceled, bool accepted, Button* last_pressed) {
    EXPECT_EQ(canceled, canceled_);
    canceled_ = false;
    EXPECT_EQ(accepted, accepted_);
    accepted_ = false;
    EXPECT_EQ(last_pressed, last_pressed_button_);
    last_pressed_button_ = NULL;
  }

  void TearDown() {
    closeable_ = true;
    GetWidget()->Close();
  }

  void set_title(const base::string16& title) { title_ = title; }

 private:
  bool canceled_;
  bool accepted_;
  // Prevent the dialog from closing, for repeated ok and cancel button clicks.
  bool closeable_;
  Button* last_pressed_button_;
  base::string16 title_;

  DISALLOW_COPY_AND_ASSIGN(TestDialog);
};

class DialogTest : public ViewsTestBase {
 public:
  DialogTest() : dialog_(NULL) {}
  ~DialogTest() override {}

  void SetUp() override {
    ViewsTestBase::SetUp();
    dialog_ = new TestDialog();
    DialogDelegate::CreateDialogWidget(dialog_, GetContext(), NULL)->Show();
  }

  void TearDown() override {
    dialog_->TearDown();
    ViewsTestBase::TearDown();
  }

  TestDialog* dialog() const { return dialog_; }

 private:
  TestDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(DialogTest);
};

}  // namespace

TEST_F(DialogTest, DefaultButtons) {
  DialogClientView* client_view = dialog()->GetDialogClientView();
  LabelButton* ok_button = client_view->ok_button();

  // DialogDelegate's default button (ok) should be default (and handle enter).
  EXPECT_EQ(ui::DIALOG_BUTTON_OK, dialog()->GetDefaultDialogButton());
  dialog()->PressEnterAndCheckStates(ok_button);

  // Focus another button in the dialog, it should become the default.
  LabelButton* button_1 = new LabelButton(dialog(), base::string16());
  client_view->AddChildView(button_1);
  client_view->OnWillChangeFocus(ok_button, button_1);
  EXPECT_TRUE(button_1->is_default());
  dialog()->PressEnterAndCheckStates(button_1);

  // Focus a Checkbox (not a push button), OK should become the default again.
  Checkbox* checkbox = new Checkbox(base::string16());
  client_view->AddChildView(checkbox);
  client_view->OnWillChangeFocus(button_1, checkbox);
  EXPECT_FALSE(button_1->is_default());
  dialog()->PressEnterAndCheckStates(ok_button);

  // Focus yet another button in the dialog, it should become the default.
  LabelButton* button_2 = new LabelButton(dialog(), base::string16());
  client_view->AddChildView(button_2);
  client_view->OnWillChangeFocus(checkbox, button_2);
  EXPECT_FALSE(button_1->is_default());
  EXPECT_TRUE(button_2->is_default());
  dialog()->PressEnterAndCheckStates(button_2);

  // Focus nothing, OK should become the default again.
  client_view->OnWillChangeFocus(button_2, NULL);
  EXPECT_FALSE(button_1->is_default());
  EXPECT_FALSE(button_2->is_default());
  dialog()->PressEnterAndCheckStates(ok_button);
}

TEST_F(DialogTest, AcceptAndCancel) {
  DialogClientView* client_view = dialog()->GetDialogClientView();
  LabelButton* ok_button = client_view->ok_button();
  LabelButton* cancel_button = client_view->cancel_button();

  // Check that return/escape accelerators accept/cancel dialogs.
  const ui::KeyEvent return_key(
      ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
  dialog()->GetFocusManager()->OnKeyEvent(return_key);
  dialog()->CheckAndResetStates(false, true, NULL);
  const ui::KeyEvent escape_key(
      ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  dialog()->GetFocusManager()->OnKeyEvent(escape_key);
  dialog()->CheckAndResetStates(true, false, NULL);

  // Check ok and cancel button behavior on a directed return key events.
  ok_button->OnKeyPressed(return_key);
  dialog()->CheckAndResetStates(false, true, NULL);
  cancel_button->OnKeyPressed(return_key);
  dialog()->CheckAndResetStates(true, false, NULL);

  // Check that return accelerators cancel dialogs if cancel is focused.
  cancel_button->RequestFocus();
  dialog()->GetFocusManager()->OnKeyEvent(return_key);
  dialog()->CheckAndResetStates(true, false, NULL);
}

TEST_F(DialogTest, RemoveDefaultButton) {
  // Removing buttons from the dialog here should not cause a crash on close.
  delete dialog()->GetDialogClientView()->ok_button();
  delete dialog()->GetDialogClientView()->cancel_button();
}

TEST_F(DialogTest, HitTest_HiddenTitle) {
  // Ensure that BubbleFrameView hit-tests as expected when the title is hidden.
  const NonClientView* view = dialog()->GetWidget()->non_client_view();
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(view->frame_view());
  const int border = frame->bubble_border()->GetBorderThickness();

  struct {
    const int point;
    const int hit;
  } cases[] = {
    { border,      HTSYSMENU },
    { border + 10, HTSYSMENU },
    { border + 20, HTCLIENT  },
    { border + 50, HTCLIENT  },
    { border + 60, HTCLIENT  },
    { 1000,        HTNOWHERE },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    gfx::Point point(cases[i].point, cases[i].point);
    EXPECT_EQ(cases[i].hit, frame->NonClientHitTest(point))
        << " with border: " << border << ", at point " << cases[i].point;
  }
}

TEST_F(DialogTest, HitTest_WithTitle) {
  // Ensure that BubbleFrameView hit-tests as expected when the title is shown.
  const NonClientView* view = dialog()->GetWidget()->non_client_view();
  dialog()->set_title(base::ASCIIToUTF16("Title"));
  dialog()->GetWidget()->UpdateWindowTitle();
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(view->frame_view());
  const int border = frame->bubble_border()->GetBorderThickness();

  struct {
    const int point;
    const int hit;
  } cases[] = {
    { border,      HTSYSMENU },
    { border + 10, HTSYSMENU },
    { border + 20, HTCAPTION },
    { border + 50, HTCLIENT  },
    { border + 60, HTCLIENT  },
    { 1000,        HTNOWHERE },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    gfx::Point point(cases[i].point, cases[i].point);
    EXPECT_EQ(cases[i].hit, frame->NonClientHitTest(point))
        << " with border: " << border << ", at point " << cases[i].point;
  }
}

TEST_F(DialogTest, BoundsAccommodateTitle) {
  TestDialog* dialog2(new TestDialog());
  dialog2->set_title(base::ASCIIToUTF16("Title"));
  DialogDelegate::CreateDialogWidget(dialog2, GetContext(), NULL);

  // Titled dialogs have taller initial frame bounds than untitled dialogs.
  View* frame1 = dialog()->GetWidget()->non_client_view()->frame_view();
  View* frame2 = dialog2->GetWidget()->non_client_view()->frame_view();
  EXPECT_LT(frame1->GetPreferredSize().height(),
            frame2->GetPreferredSize().height());

  // Giving the default test dialog a title will yield the same bounds.
  dialog()->set_title(base::ASCIIToUTF16("Title"));
  dialog()->GetWidget()->UpdateWindowTitle();
  EXPECT_EQ(frame1->GetPreferredSize().height(),
            frame2->GetPreferredSize().height());

  dialog2->TearDown();
}

}  // namespace views
