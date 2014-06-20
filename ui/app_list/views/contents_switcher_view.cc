// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_switcher_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

const int kPreferredHeight = 32;
const int kButtonSpacing = 4;

}  // namespace

ContentsSwitcherView::ContentsSwitcherView(ContentsView* contents_view)
    : contents_view_(contents_view), buttons_(new views::View) {
  AddChildView(buttons_);

  buttons_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kButtonSpacing));
}

ContentsSwitcherView::~ContentsSwitcherView() {}

void ContentsSwitcherView::AddSwitcherButton(int resource_id, int page_index) {
  views::ImageButton* button = new views::ImageButton(this);
  if (resource_id) {
    button->SetImage(
        views::CustomButton::STATE_NORMAL,
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id));
  }
  button->set_tag(page_index);
  buttons_->AddChildView(button);
}

gfx::Size ContentsSwitcherView::GetPreferredSize() const {
  return gfx::Size(buttons_->GetPreferredSize().width(), kPreferredHeight);
}

void ContentsSwitcherView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  // Makes |buttons_| horizontally center and vertically fill.
  gfx::Size buttons_size(buttons_->GetPreferredSize());
  gfx::Rect buttons_bounds(rect.CenterPoint().x() - buttons_size.width() / 2,
                           rect.y(),
                           buttons_size.width(),
                           rect.height());
  buttons_->SetBoundsRect(gfx::IntersectRects(rect, buttons_bounds));
}

void ContentsSwitcherView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  contents_view_->SetActivePage(sender->tag());
}

}  // namespace app_list
