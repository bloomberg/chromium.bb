// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_tile_item_view.h"

#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/search_result_container_view.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace app_list {

SearchResultTileItemView::SearchResultTileItemView(
    SearchResultContainerView* result_container,
    AppListViewDelegate* view_delegate)
    : result_container_(result_container),
      item_(nullptr),
      view_delegate_(view_delegate) {
  // When |item_| is null, the tile is invisible. Calling SetSearchResult with a
  // non-null item makes the tile visible.
  SetVisible(false);

  set_context_menu_controller(this);
}

SearchResultTileItemView::~SearchResultTileItemView() {
  if (item_)
    item_->RemoveObserver(this);
}

void SearchResultTileItemView::SetSearchResult(SearchResult* item) {
  // Handle the case where this may be called from a nested run loop while its
  // context menu is showing. This cancels the menu (it's for the old item).
  context_menu_runner_.reset();

  SetVisible(item != NULL);

  SearchResult* old_item = item_;
  if (old_item)
    old_item->RemoveObserver(this);

  item_ = item;

  if (!item)
    return;

  item_->AddObserver(this);

  SetTitle(item_->title());

  // Only refresh the icon if it's different from the old one. This prevents
  // flickering.
  if (old_item == NULL ||
      !item->icon().BackedBySameObjectAs(old_item->icon())) {
    OnIconChanged();
  }
}

void SearchResultTileItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  view_delegate_->OpenSearchResult(item_, false, event.flags());
}

bool SearchResultTileItemView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    view_delegate_->OpenSearchResult(item_, false, event.flags());
    return true;
  }

  return false;
}

void SearchResultTileItemView::OnIconChanged() {
  SetIcon(item_->icon());
}

void SearchResultTileItemView::OnBadgeIconChanged() {
  SetBadgeIcon(item_->badge_icon());
}

void SearchResultTileItemView::OnResultDestroying() {
  // The menu comes from |item_|. If we're showing a menu we need to cancel it.
  context_menu_runner_.reset();

  if (item_)
    item_->RemoveObserver(this);

  SetSearchResult(nullptr);
}

void SearchResultTileItemView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |item_| could be null when result list is changing.
  if (!item_)
    return;

  ui::MenuModel* menu_model = item_->GetContextMenuModel();
  if (!menu_model)
    return;

  if (!selected())
    result_container_->ClearSelectedIndex();

  context_menu_runner_.reset(
      new views::MenuRunner(menu_model, views::MenuRunner::HAS_MNEMONICS));
  context_menu_runner_->RunMenuAt(GetWidget(), nullptr,
                                  gfx::Rect(point, gfx::Size()),
                                  views::MENU_ANCHOR_TOPLEFT, source_type);
}

}  // namespace app_list
