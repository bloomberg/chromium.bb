// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"  // ASCIIToUTF16
#include "ui/aura/window.h"
#include "ui/aura_shell/toplevel_frame_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// Default window position.
const int kWindowLeft = 170;
const int kWindowTop = 200;

// Default window size.
const int kWindowWidth = 400;
const int kWindowHeight = 400;

// A window showing samples of commonly used widgets.
class WidgetsWindow : public views::WidgetDelegateView {
 public:
  WidgetsWindow();
  virtual ~WidgetsWindow();

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

 private:
  views::NativeTextButton* button_;
  views::NativeTextButton* disabled_button_;
  views::Checkbox* checkbox_;
  views::Checkbox* checkbox_disabled_;
  views::Checkbox* checkbox_checked_;
  views::Checkbox* checkbox_checked_disabled_;
  views::RadioButton* radio_button_;
  views::RadioButton* radio_button_disabled_;
  views::RadioButton* radio_button_selected_;
  views::RadioButton* radio_button_selected_disabled_;
};

WidgetsWindow::WidgetsWindow()
    : button_(new views::NativeTextButton(NULL, ASCIIToUTF16("Button"))),
      disabled_button_(
          new views::NativeTextButton(NULL, ASCIIToUTF16("Disabled button"))),
      checkbox_(new views::Checkbox(ASCIIToUTF16("Checkbox"))),
      checkbox_disabled_(new views::Checkbox(
          ASCIIToUTF16("Checkbox disabled"))),
      checkbox_checked_(new views::Checkbox(ASCIIToUTF16("Checkbox checked"))),
      checkbox_checked_disabled_(new views::Checkbox(
          ASCIIToUTF16("Checkbox checked disabled"))),
      radio_button_(new views::RadioButton(ASCIIToUTF16("Radio button"), 0)),
      radio_button_disabled_(new views::RadioButton(
          ASCIIToUTF16("Radio button disabled"), 0)),
      radio_button_selected_(new views::RadioButton(
          ASCIIToUTF16("Radio button selected"), 0)),
      radio_button_selected_disabled_(new views::RadioButton(
          ASCIIToUTF16("Radio button selected disabled"), 1)) {
  AddChildView(button_);
  disabled_button_->SetEnabled(false);
  AddChildView(disabled_button_);
  AddChildView(checkbox_);
  checkbox_disabled_->SetEnabled(false);
  AddChildView(checkbox_disabled_);
  checkbox_checked_->SetChecked(true);
  AddChildView(checkbox_checked_);
  checkbox_checked_disabled_->SetChecked(true);
  checkbox_checked_disabled_->SetEnabled(false);
  AddChildView(checkbox_checked_disabled_);
  AddChildView(radio_button_);
  radio_button_disabled_->SetEnabled(false);
  AddChildView(radio_button_disabled_);
  radio_button_selected_->SetChecked(true);
  AddChildView(radio_button_selected_);
  radio_button_selected_disabled_->SetChecked(true);
  radio_button_selected_disabled_->SetEnabled(false);
  AddChildView(radio_button_selected_disabled_);
}

WidgetsWindow::~WidgetsWindow() {
}

void WidgetsWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(SK_ColorWHITE, GetLocalBounds());
}

void WidgetsWindow::Layout() {
  const int kVerticalPad = 5;
  int left = 5;
  int top = kVerticalPad;
  for (Views::const_iterator it = children_begin();
       it != children_end();
       ++it) {
    views::View* view = *it;
    gfx::Size preferred = view->GetPreferredSize();
    view->SetBounds(left, top, preferred.width(), preferred.height());
    top += preferred.height() + kVerticalPad;
  }
}

gfx::Size WidgetsWindow::GetPreferredSize() {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

views::View* WidgetsWindow::GetContentsView() {
  return this;
}

string16 WidgetsWindow::GetWindowTitle() const {
  return ASCIIToUTF16("Examples: Widgets");
}

views::NonClientFrameView* WidgetsWindow::CreateNonClientFrameView() {
  return new aura_shell::internal::ToplevelFrameView;
}

}  // namespace

namespace aura_shell {
namespace examples {

void CreateWidgetsWindow() {
  gfx::Rect bounds(kWindowLeft, kWindowTop, kWindowWidth, kWindowHeight);
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(new WidgetsWindow, bounds);
  widget->GetNativeView()->SetName("WidgetsWindow");
  widget->Show();
}

}  // namespace examples
}  // namespace aura_shell
