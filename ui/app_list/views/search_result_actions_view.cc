// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_actions_view.h"

#include "ui/app_list/views/search_result_actions_view_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

SearchResultActionsView::SearchResultActionsView(
    SearchResultActionsViewDelegate* delegate)
    : delegate_(delegate) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 10, 0));
}

SearchResultActionsView::~SearchResultActionsView() {}

void SearchResultActionsView::SetActions(const SearchResult::Actions& actions) {
  RemoveAllChildViews(true);

  for (size_t i = 0; i < actions.size(); ++i) {
    const SearchResult::Action& action = actions.at(i);
    if (action.label_text.empty())
      CreateImageButton(action);
    else
      CreateBlueButton(action);
  }

  PreferredSizeChanged();
}

void SearchResultActionsView::CreateImageButton(
    const SearchResult::Action& action) {
  views::ImageButton* button = new views::ImageButton(this);
  button->set_border(views::Border::CreateEmptyBorder(0, 4, 0, 4));
  button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                            views::ImageButton::ALIGN_MIDDLE);
  button->SetImage(views::CustomButton::STATE_NORMAL, &action.base_image);
  button->SetImage(views::CustomButton::STATE_HOVERED, &action.hover_image);
  button->SetImage(views::CustomButton::STATE_PRESSED, &action.pressed_image);
  button->SetTooltipText(action.tooltip_text);
  AddChildView(button);
}

void SearchResultActionsView::CreateBlueButton(
    const SearchResult::Action& action) {
  views::BlueButton* button = new views::BlueButton(this, action.label_text);
  button->SetTooltipText(action.tooltip_text);
  button->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::SmallBoldFont));
  AddChildView(button);
}

void SearchResultActionsView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (!delegate_)
    return;

  const int index = GetIndexOf(sender);
  DCHECK_NE(-1, index);
  delegate_->OnSearchResultActionActivated(index, event.flags());
}

}  // namespace app_list
