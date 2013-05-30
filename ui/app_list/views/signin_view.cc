// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/signin_view.h"

#include "ui/app_list/signin_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"

namespace {

const int kTopPadding = 40;
const int kBottomPadding = 40;
const int kLeftPadding = 40;
const int kRightPadding = 40;
const int kHeadingPadding = 30;
const int kButtonPadding = 40;

const int kTitleFontSize = 18;
const int kTextFontSize = 13;
const int kButtonFontSize = 12;

}  // namespace

namespace app_list {

SigninView::SigninView(SigninDelegate* delegate, int width)
    : delegate_(delegate) {
  if (!delegate_)
    return;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::Font& base_font = rb.GetFont(ui::ResourceBundle::BaseFont);
  title_font_.reset(new gfx::Font(base_font.GetFontName(), kTitleFontSize));
  text_font_.reset(new gfx::Font(base_font.GetFontName(), kTextFontSize));
  button_font_.reset(new gfx::Font(base_font.GetFontName(), kButtonFontSize));

  int title_descender = title_font_->GetHeight() - title_font_->GetBaseline();
  int text_descender = text_font_->GetHeight() - text_font_->GetBaseline();

  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(kTopPadding, kLeftPadding, kBottomPadding - text_descender,
                    kRightPadding);
  SetLayoutManager(layout);

  const int kNormalSetId = 0;
  views::ColumnSet* columns = layout->AddColumnSet(kNormalSetId);
  columns->AddColumn(views::GridLayout::FILL,
                     views::GridLayout::FILL,
                     1,
                     views::GridLayout::USE_PREF,
                     0,
                     0);

  const int kButtonSetId = 1;
  columns = layout->AddColumnSet(kButtonSetId);
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::FILL,
                     1,
                     views::GridLayout::USE_PREF,
                     0,
                     0);

  views::Label* heading = new views::Label(delegate_->GetSigninHeading());
  heading->SetFont(*title_font_);
  heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  layout->StartRow(0, kNormalSetId);
  layout->AddView(heading);

  views::Label* text = new views::Label(delegate_->GetSigninText());
  text->SetFont(*text_font_);
  text->SetMultiLine(true);
  text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRowWithPadding(0, kNormalSetId, 0,
                              kHeadingPadding - title_descender);
  layout->AddView(text);

  views::BlueButton* signin_button = new views::BlueButton(
      this,
      delegate_->GetSigninButtonText());
  signin_button->SetFont(*button_font_);
  layout->StartRowWithPadding(0, kButtonSetId, 0,
                              kButtonPadding - text_descender);
  layout->AddView(signin_button);

  layout->StartRow(1, kNormalSetId);
  learn_more_link_ = new views::Link(delegate_->GetLearnMoreLinkText());
  learn_more_link_->set_listener(this);
  learn_more_link_->SetFont(*text_font_);
  learn_more_link_->SetUnderline(false);
  layout->AddView(learn_more_link_,
                  1,
                  1,
                  views::GridLayout::LEADING,
                  views::GridLayout::TRAILING);

  layout->StartRow(0, kNormalSetId);
  settings_link_ = new views::Link(delegate_->GetSettingsLinkText());
  settings_link_->set_listener(this);
  settings_link_->SetFont(*text_font_);
  settings_link_->SetUnderline(false);
  layout->AddView(settings_link_,
                  1,
                  1,
                  views::GridLayout::LEADING,
                  views::GridLayout::TRAILING);
}

SigninView::~SigninView() {
}

void SigninView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (delegate_)
    delegate_->ShowSignin();
}

void SigninView::LinkClicked(views::Link* source, int event_flags) {
  if (delegate_) {
    if (source == learn_more_link_)
      delegate_->OpenLearnMore();
    else if (source == settings_link_)
      delegate_->OpenSettings();
    else
      NOTREACHED();
  }
}

}  // namespace app_list
