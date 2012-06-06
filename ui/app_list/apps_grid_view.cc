// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/apps_grid_view.h"

#include <algorithm>

#include "ui/app_list/app_list_item_view.h"
#include "ui/app_list/pagination_model.h"
#include "ui/views/border.h"

namespace {

// Padding space in pixels for fixed layout.
const int kLeftRightPadding = 20;
const int kTopPadding = 1;

// Preferred tile size when showing in fixed layout.
const int kPreferredTileWidth = 88;
const int kPreferredTileHeight = 98;

}  // namespace

namespace app_list {

AppsGridView::AppsGridView(views::ButtonListener* listener,
                                   PaginationModel* pagination_model)
    : model_(NULL),
      listener_(listener),
      pagination_model_(pagination_model),
      cols_(0),
      rows_per_page_(0),
      selected_item_index_(-1) {
  set_focusable(true);
  pagination_model_->AddObserver(this);
}

AppsGridView::~AppsGridView() {
  if (model_)
    model_->RemoveObserver(this);
  pagination_model_->RemoveObserver(this);
}

void AppsGridView::SetLayout(int icon_size, int cols, int rows_per_page) {
  icon_size_.SetSize(icon_size, icon_size);
  cols_ = cols;
  rows_per_page_ = rows_per_page;

  set_border(views::Border::CreateEmptyBorder(kTopPadding,
                                              kLeftRightPadding,
                                              0,
                                              kLeftRightPadding));
}

void AppsGridView::SetModel(AppListModel::Apps* model) {
  if (model_)
    model_->RemoveObserver(this);

  model_ = model;
  if (model_)
    model_->AddObserver(this);
  Update();
}

void AppsGridView::SetSelectedItem(AppListItemView* item) {
  int index = GetIndexOf(item);
  if (index >= 0)
    SetSelectedItemByIndex(index);
}

void AppsGridView::ClearSelectedItem(AppListItemView* item) {
  int index = GetIndexOf(item);
  if (index == selected_item_index_)
    SetSelectedItemByIndex(-1);
}

bool AppsGridView::IsSelectedItem(const AppListItemView* item) const {
  return selected_item_index_ != -1 &&
      selected_item_index_ == GetIndexOf(item);
}

void AppsGridView::EnsureItemVisible(const AppListItemView* item) {
  int index = GetIndexOf(item);
  if (index >= 0 && tiles_per_page())
    pagination_model_->SelectPage(index / tiles_per_page());
}

gfx::Size AppsGridView::GetPreferredSize() {
  gfx::Insets insets(GetInsets());
  gfx::Size tile_size = gfx::Size(kPreferredTileWidth, kPreferredTileHeight);
  return gfx::Size(tile_size.width() * cols_ + insets.width(),
                   tile_size.height() * rows_per_page_ + insets.height());
}

void AppsGridView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty() || child_count() == 0 || !tiles_per_page())
    return;

  gfx::Size tile_size(kPreferredTileWidth, kPreferredTileHeight);


  pagination_model_->SetTotalPages((child_count() - 1) / tiles_per_page() + 1);
  if (pagination_model_->selected_page() < 0)
    pagination_model_->SelectPage(0);

  gfx::Rect grid_rect = rect.Center(
      gfx::Size(tile_size.width() * cols_,
                tile_size.height() * rows_per_page_));
  grid_rect = grid_rect.Intersect(rect);

  // Layouts items.
  const int page = pagination_model_->selected_page();
  const int first_visible_index = page * tiles_per_page();
  const int last_visible_index = (page + 1) * tiles_per_page() - 1;
  gfx::Rect current_tile(grid_rect.origin(), tile_size);
  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);
    static_cast<AppListItemView*>(view)->SetIconSize(icon_size_);

    if (i < first_visible_index || i > last_visible_index) {
      view->SetVisible(false);
      continue;
    }

    view->SetBoundsRect(current_tile);
    view->SetVisible(rect.Contains(current_tile));

    current_tile.Offset(tile_size.width(), 0);
    if ((i + 1) % cols_ == 0) {
      current_tile.set_x(grid_rect.x());
      current_tile.set_y(current_tile.y() + tile_size.height());
    }
  }
}

bool AppsGridView::OnKeyPressed(const views::KeyEvent& event) {
  bool handled = false;
  if (selected_item_index_ >= 0)
    handled = GetItemViewAtIndex(selected_item_index_)->OnKeyPressed(event);

  if (!handled) {
    switch (event.key_code()) {
      case ui::VKEY_LEFT:
        SetSelectedItemByIndex(std::max(selected_item_index_ - 1, 0));
        return true;
      case ui::VKEY_RIGHT:
        SetSelectedItemByIndex(std::min(selected_item_index_ + 1,
                                        child_count() - 1));
        return true;
      case ui::VKEY_UP:
        SetSelectedItemByIndex(std::max(selected_item_index_ - cols_,
                                        0));
        return true;
      case ui::VKEY_DOWN:
        if (selected_item_index_ < 0) {
          SetSelectedItemByIndex(0);
        } else {
          SetSelectedItemByIndex(std::min(selected_item_index_ + cols_,
                                          child_count() - 1));
        }
        return true;
      case ui::VKEY_PRIOR: {
        SetSelectedItemByIndex(
            std::max(selected_item_index_ - tiles_per_page(),
                     0));
        return true;
      }
      case ui::VKEY_NEXT: {
        if (selected_item_index_ < 0) {
          SetSelectedItemByIndex(0);
        } else {
          SetSelectedItemByIndex(
              std::min(selected_item_index_ + tiles_per_page(),
                       child_count() - 1));
        }
      }
      default:
        break;
    }
  }

  return handled;
}

bool AppsGridView::OnKeyReleased(const views::KeyEvent& event) {
  bool handled = false;
  if (selected_item_index_ >= 0)
    handled = GetItemViewAtIndex(selected_item_index_)->OnKeyReleased(event);

  return handled;
}

void AppsGridView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Override to not paint focus frame.
}

void AppsGridView::Update() {
  selected_item_index_ = -1;
  RemoveAllChildViews(true);
  if (!model_ || model_->item_count() == 0)
    return;

  for (size_t i = 0; i < model_->item_count(); ++i)
    AddChildView(new AppListItemView(this, model_->GetItemAt(i), listener_));

  Layout();
  SchedulePaint();
}

AppListItemView* AppsGridView::GetItemViewAtIndex(int index) {
  return static_cast<AppListItemView*>(child_at(index));
}

void AppsGridView::SetSelectedItemByIndex(int index) {
  if (selected_item_index_ == index)
    return;

  if (selected_item_index_ >= 0)
    GetItemViewAtIndex(selected_item_index_)->SchedulePaint();

  if (index < 0 || index >= child_count()) {
    selected_item_index_ = -1;
  } else {
    selected_item_index_ = index;
    GetItemViewAtIndex(selected_item_index_)->SchedulePaint();

    if (tiles_per_page())
      pagination_model_->SelectPage(selected_item_index_ / tiles_per_page());
  }
}

void AppsGridView::ListItemsAdded(size_t start, size_t count) {
  for (size_t i = start; i < start + count; ++i) {
    AddChildViewAt(new AppListItemView(this, model_->GetItemAt(i), listener_),
                   i);
  }
  Layout();
  SchedulePaint();
}

void AppsGridView::ListItemsRemoved(size_t start, size_t count) {
  for (size_t i = 0; i < count; ++i)
    delete child_at(start);

  Layout();
  SchedulePaint();
}

void AppsGridView::ListItemsChanged(size_t start, size_t count) {
  NOTREACHED();
}

void AppsGridView::TotalPagesChanged() {
}

void AppsGridView::SelectedPageChanged(int old_selected, int new_selected) {
  Layout();
}

}  // namespace app_list
