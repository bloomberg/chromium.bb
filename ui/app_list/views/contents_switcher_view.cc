// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_switcher_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

namespace {

const int kButtonImageSize = 32;
const int kButtonSpacing = 4;

class ContentsPageIndicatorView : public views::View {
 public:
  ContentsPageIndicatorView() {};
  virtual ~ContentsPageIndicatorView() {};

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(0, kContentsSwitcherSeparatorHeight);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentsPageIndicatorView);
};

}  // namespace

ContentsSwitcherView::ContentsSwitcherView(ContentsView* contents_view)
    : contents_view_(contents_view) {
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kButtonSpacing);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout);
}

ContentsSwitcherView::~ContentsSwitcherView() {}

void ContentsSwitcherView::AddSwitcherButton(int resource_id, int page_index) {
  views::ImageButton* button = new views::ImageButton(this);
  button->SetPreferredSize(gfx::Size(kButtonImageSize, kButtonImageSize));
  if (resource_id) {
    button->SetImage(
        views::CustomButton::STATE_NORMAL,
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id));
  }
  button->set_tag(page_index);

  // Add an indicator for the current launcher page.
  app_list::ContentsPageIndicatorView* indicator =
      new app_list::ContentsPageIndicatorView();
  indicator->set_background(
      views::Background::CreateSolidBackground(app_list::kPagerSelectedColor));
  indicator->SetVisible(false);
  page_active_indicators_.push_back(indicator);

  // A container view that will consume space when its child is not visible.
  // TODO(calamity): Remove this once BoxLayout supports space-consuming
  // invisible views.
  views::View* indicator_container = new views::View();
  indicator_container->SetLayoutManager(new views::FillLayout());
  indicator_container->AddChildView(indicator);

  // View containing the indicator view and image button.
  views::View* button_container = new views::View();
  button_container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  button_container->AddChildView(indicator_container);
  button_container->AddChildView(button);

  AddChildView(button_container);
}

void ContentsSwitcherView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  contents_view_->SetActivePage(sender->tag());
}

void ContentsSwitcherView::TotalPagesChanged() {
}

void ContentsSwitcherView::SelectedPageChanged(int old_selected,
                                               int new_selected) {
  // Makes the indicator visible when it is first drawn and when the
  // selected page is changed.
  int num_indicators = static_cast<int>(page_active_indicators_.size());
  if (old_selected >= 0 && old_selected < num_indicators)
    page_active_indicators_[old_selected]->SetVisible(false);

  if (new_selected >= 0 && new_selected < num_indicators)
    page_active_indicators_[new_selected]->SetVisible(true);
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
