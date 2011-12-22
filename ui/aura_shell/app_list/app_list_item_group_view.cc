// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/app_list/app_list_item_group_view.h"

#include "ui/aura_shell/app_list/app_list_item_group_model.h"
#include "ui/aura_shell/app_list/app_list_item_view.h"
#include "ui/views/layout/grid_layout.h"

namespace aura_shell {

AppListItemGroupView::AppListItemGroupView(AppListItemGroupModel* model,
                                           AppListItemViewListener* listener)
    : model_(model),
      listener_(listener),
      tiles_per_row_(0),
      focused_index_(0) {
  Update();
}

AppListItemGroupView::~AppListItemGroupView() {
}

void AppListItemGroupView::SetTilesPerRow(int tiles_per_row) {
  if (tiles_per_row_ == tiles_per_row)
    return;

  tiles_per_row_ = tiles_per_row;
  Update();
}

void AppListItemGroupView::Update() {
  RemoveAllChildViews(true);
  if (model_->item_count() == 0 || tiles_per_row_ == 0)
    return;

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int kTileColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kTileColumnSetId);
  for (int i = 0; i < tiles_per_row_; ++i) {
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::USE_PREF, 0, 0);
  }

  for (int i = 0; i < model_->item_count(); ++i) {
    if (i % tiles_per_row_ == 0)
      layout->StartRow(0, kTileColumnSetId);

    layout->AddView(new AppListItemView(model_->GetItem(i), listener_));
  }
}

views::View* AppListItemGroupView::GetFocusedTile() {
  return focused_index_ < child_count() ? child_at(focused_index_) : NULL;
}

void AppListItemGroupView::UpdateFocusedTile(views::View* tile) {
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i) == tile) {
      focused_index_ = i;
      break;
    }
  }
}

void AppListItemGroupView::SetFocusedTileByIndex(int index) {
  index = std::max(0, std::min(child_count() - 1, index));
  if (index != focused_index_)
    child_at(index)->RequestFocus();
}

bool AppListItemGroupView::OnKeyPressed(const views::KeyEvent& event) {
  if (!event.IsControlDown() && !event.IsShiftDown() && !event.IsAltDown()) {
    // Arrow keys navigates in tile grid.
    switch (event.key_code()) {
      case ui::VKEY_LEFT:
        if (focused_index_ > 0)
          SetFocusedTileByIndex(focused_index_ - 1);
        return true;
      case ui::VKEY_RIGHT:
        if (focused_index_ + 1 < child_count())
          SetFocusedTileByIndex(focused_index_ + 1);
        return true;
      case ui::VKEY_UP:
        if (focused_index_ - tiles_per_row_ >= 0)
          SetFocusedTileByIndex(focused_index_ - tiles_per_row_);
        return true;
      case ui::VKEY_DOWN:
        if (focused_index_ + tiles_per_row_ < child_count())
          SetFocusedTileByIndex(focused_index_ + tiles_per_row_);
        return true;
      default:
        break;
    }
  }

  return false;
}

void AppListItemGroupView::ListItemsAdded(int start, int count) {
  Update();
}

void AppListItemGroupView::ListItemsRemoved(int start, int count) {
  Update();
}

void AppListItemGroupView::ListItemsChanged(int start, int count) {
  NOTREACHED();
}

}  // namespace aura_shell
