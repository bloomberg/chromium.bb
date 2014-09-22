// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_switcher_view.h"

#include "ui/app_list/views/contents_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

const int kButtonSpacing = 4;
const int kMinimumHeight = 39;

}  // namespace

ContentsSwitcherView::ContentsSwitcherView(ContentsView* contents_view)
    : contents_view_(contents_view) {
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kButtonSpacing);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_minimum_cross_axis_size(kMinimumHeight);
  SetLayoutManager(layout);
}

ContentsSwitcherView::~ContentsSwitcherView() {}

void ContentsSwitcherView::AddSwitcherButton(int resource_id, int page_index) {
  views::ImageButton* button = new views::ImageButton(this);
  button->SetMinimumImageSize(gfx::Size(kMinimumHeight, kMinimumHeight));
  button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                            views::ImageButton::ALIGN_MIDDLE);
  button->SetImage(
      views::CustomButton::STATE_NORMAL,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id));
  button->set_tag(page_index);

  AddChildView(button);
}

void ContentsSwitcherView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  contents_view_->SetActivePage(sender->tag());
}

void ContentsSwitcherView::TotalPagesChanged() {
}

void ContentsSwitcherView::SelectedPageChanged(int old_selected,
                                               int new_selected) {
  // TODO(mgiuca): Visually indicate which page is now selected.
}

void ContentsSwitcherView::TransitionStarted() {
}

void ContentsSwitcherView::TransitionChanged() {
  // Change the indicator during a launcher page transition.
  const PaginationModel& pm = contents_view_->pagination_model();
  int old_selected = pm.selected_page();
  int new_selected = pm.transition().target_page;
  if (pm.IsRevertingCurrentTransition()) {
    // Swap the direction if the transition is reversed.
    old_selected = pm.transition().target_page;
    new_selected = pm.selected_page();
  }

  SelectedPageChanged(old_selected, new_selected);
}

}  // namespace app_list
