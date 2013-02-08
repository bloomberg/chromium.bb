// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/signin_view.h"

#include "ui/app_list/signin_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"

namespace {

const int kTopPadding = 10;
const int kLeftPadding = 20;
const int kRightPadding = 30;
const int kHeadingPadding = 30;
const int kButtonPadding = 100;

}  // namespace

namespace app_list {

SigninView::SigninView(SigninDelegate* delegate, int width)
    : delegate_(delegate) {
  if (!delegate_)
    return;

  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(kTopPadding, kLeftPadding, 0, kRightPadding);
  SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL,
                     views::GridLayout::FILL,
                     1,
                     views::GridLayout::USE_PREF,
                     0,
                     0);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::Label* heading = new views::Label(delegate_->GetSigninHeading());
  heading->SetFont(rb.GetFont(ui::ResourceBundle::LargeFont));
  heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  layout->StartRow(0, 0);
  layout->AddView(heading);

  views::Label* text = new views::Label(delegate_->GetSigninText());
  text->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
  text->SetMultiLine(true);
  text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRowWithPadding(0, 0, 0, kHeadingPadding);
  layout->AddView(text);

  views::LabelButton* signin_button = new views::LabelButton(
      this,
      delegate_->GetSigninButtonText());
  signin_button->SetNativeTheme(true);
  layout->StartRowWithPadding(0, 0, 0, kButtonPadding);
  layout->AddView(signin_button);
}

SigninView::~SigninView() {
}

void SigninView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (delegate_)
    delegate_->ShowSignin();
}

}  // namespace app_list
