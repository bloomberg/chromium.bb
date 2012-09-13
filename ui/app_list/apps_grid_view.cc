// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/apps_grid_view.h"

#include <algorithm>

#include "ui/app_list/app_list_item_view.h"
#include "ui/app_list/pagination_model.h"
#include "ui/base/events/event.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"

namespace {

// Padding space in pixels for fixed layout.
const int kLeftRightPadding = 20;
const int kTopPadding = 1;

// Padding space in pixels between pages.
const int kPagePadding = 40;

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
  if (index >= 0 && tiles_per_page()) {
    pagination_model_->SelectPage(index / tiles_per_page(),
                                  false /* animate */);
  }
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

  gfx::Rect grid_rect = rect.Center(
      gfx::Size(tile_size.width() * cols_,
                tile_size.height() * rows_per_page_));
  grid_rect = grid_rect.Intersect(rect);

  // Page width including padding pixels. A tile.x + page_width means the same
  // tile slot in the next page.
  const int page_width = grid_rect.width() + kPagePadding;

  // If there is a transition, calculates offset for current and target page.
  const int current_page = pagination_model_->selected_page();
  const PaginationModel::Transition& transition =
      pagination_model_->transition();
  const bool is_valid =
      pagination_model_->is_valid_page(transition.target_page);

  // Transition to right means negative offset.
  const int dir = transition.target_page > current_page ? -1 : 1;
  const int transition_offset = is_valid ?
      transition.progress * page_width * dir : 0;

  const int first_visible_index = current_page * tiles_per_page();
  const int last_visible_index = (current_page + 1) * tiles_per_page() - 1;
  gfx::Rect tile_slot(grid_rect.origin(), tile_size);
  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);

    // Decides an x_offset for current item.
    int x_offset = 0;
    if (i < first_visible_index)
      x_offset = -page_width;
    else if (i > last_visible_index)
      x_offset = page_width;

    int page = i / tiles_per_page();
    if (is_valid) {
      if (page == current_page || page == transition.target_page)
        x_offset += transition_offset;
    } else {
      const int col = i % cols_;
      if (transition_offset > 0)
        x_offset += transition_offset * col;
      else
        x_offset += transition_offset * (cols_ - col - 1);
    }

    gfx::Rect adjusted_slot(tile_slot);
    adjusted_slot.Offset(x_offset, 0);
    view->SetBoundsRect(adjusted_slot);

    tile_slot.Offset(tile_size.width(), 0);
    if ((i + 1) % tiles_per_page() == 0) {
      tile_slot.set_origin(grid_rect.origin());
    } else if ((i + 1) % cols_ == 0) {
      tile_slot.set_x(grid_rect.x());
      tile_slot.set_y(tile_slot.y() + tile_size.height());
    }
  }
}

bool AppsGridView::OnKeyPressed(const ui::KeyEvent& event) {
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

bool AppsGridView::OnKeyReleased(const ui::KeyEvent& event) {
  bool handled = false;
  if (selected_item_index_ >= 0)
    handled = GetItemViewAtIndex(selected_item_index_)->OnKeyReleased(event);

  return handled;
}

void AppsGridView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Override to not paint focus frame.
}

void AppsGridView::ViewHierarchyChanged(bool is_add,
                                        views::View* parent,
                                        views::View* child) {
  if (!is_add) {
    if (parent == this &&
        selected_item_index_ >= 0 &&
        GetItemViewAtIndex(selected_item_index_) == child) {
      selected_item_index_ = -1;
    }
  }
}

void AppsGridView::Update() {
  selected_item_index_ = -1;
  RemoveAllChildViews(true);
  if (!model_ || model_->item_count() == 0)
    return;

  for (size_t i = 0; i < model_->item_count(); ++i)
    AddChildView(CreateViewForItemAtIndex(i));

  UpdatePaginationModel();

  Layout();
  SchedulePaint();
}

void AppsGridView::UpdatePaginationModel() {
  pagination_model_->SetTotalPages(
      (child_count() - 1) / tiles_per_page() + 1);
}

AppListItemView* AppsGridView::CreateViewForItemAtIndex(size_t index) {
  DCHECK_LT(index, model_->item_count());
  AppListItemView* item = new AppListItemView(this,
                                              model_->GetItemAt(index),
                                              listener_);
  item->SetIconSize(icon_size_);
#if !defined(OS_WIN)
  item->SetPaintToLayer(true);
  item->SetFillsBoundsOpaquely(false);
#endif
  return item;
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
    AppListItemView* selected_view = GetItemViewAtIndex(selected_item_index_);
    selected_view->SchedulePaint();
    GetWidget()->NotifyAccessibilityEvent(
        selected_view, ui::AccessibilityTypes::EVENT_FOCUS, true);

    if (tiles_per_page()) {
      pagination_model_->SelectPage(selected_item_index_ / tiles_per_page(),
                                    true /* animate */);
    }
  }
}

void AppsGridView::ListItemsAdded(size_t start, size_t count) {
  for (size_t i = start; i < start + count; ++i)
    AddChildViewAt(CreateViewForItemAtIndex(i), i);

  UpdatePaginationModel();

  Layout();
  SchedulePaint();
}

void AppsGridView::ListItemsRemoved(size_t start, size_t count) {
  for (size_t i = 0; i < count; ++i)
    delete child_at(start);

  UpdatePaginationModel();

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

void AppsGridView::TransitionChanged() {
  // Update layout for valid page transition only since over-scroll no longer
  // animates app icons.
  const PaginationModel::Transition& transition =
      pagination_model_->transition();
  if (pagination_model_->is_valid_page(transition.target_page))
    Layout();
}

}  // namespace app_list
