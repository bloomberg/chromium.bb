// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_tile_item_list_view.h"

#include <stddef.h>

#include "base/i18n/rtl.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/search_result_tile_item_view.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Layout constants.
constexpr size_t kNumSearchResultTiles = 8;
constexpr int kHorizontalBorderSpacing = 1;
constexpr int kBetweenTileSpacing = 2;
constexpr int kTopBottomPadding = 8;

// Layout constants used when fullscreen app list feature is enabled.
constexpr size_t kMaxNumSearchResultTiles = 6;
constexpr int kItemListVerticalSpacing = 16;
constexpr int kItemListHorizontalSpacing = 12;
constexpr int kBetweenItemSpacing = 8;
constexpr int kSeparatorLeftRightPadding = 4;
constexpr int kSeparatorHeight = 36;

constexpr SkColor kSeparatorColor = SkColorSetARGBMacro(0x1F, 0x00, 0x00, 0x00);

}  // namespace

namespace app_list {

SearchResultTileItemListView::SearchResultTileItemListView(
    views::Textfield* search_box,
    AppListViewDelegate* view_delegate)
    : search_box_(search_box),
      is_play_store_app_search_enabled_(
          features::IsPlayStoreAppSearchEnabled()),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  if (is_fullscreen_app_list_enabled_) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal,
        gfx::Insets(kItemListVerticalSpacing, kItemListHorizontalSpacing),
        kBetweenItemSpacing));
    for (size_t i = 0; i < kMaxNumSearchResultTiles; ++i) {
      if (is_play_store_app_search_enabled_) {
        views::Separator* separator = new views::Separator;
        separator->SetVisible(false);
        separator->SetBorder(views::CreateEmptyBorder(
            0, kSeparatorLeftRightPadding, kSearchTileHeight - kSeparatorHeight,
            kSeparatorLeftRightPadding));
        separator->SetColor(kSeparatorColor);

        separator_views_.push_back(separator);
        AddChildView(separator);
      }

      SearchResultTileItemView* tile_item = new SearchResultTileItemView(
          this, view_delegate, false /* Not a suggested app */);
      tile_item->SetParentBackgroundColor(kCardBackgroundColorFullscreen);
      tile_views_.push_back(tile_item);
      AddChildView(tile_item);
    }
  } else {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, gfx::Insets(0, kHorizontalBorderSpacing),
        kBetweenTileSpacing));
    for (size_t i = 0; i < kNumSearchResultTiles; ++i) {
      SearchResultTileItemView* tile_item = new SearchResultTileItemView(
          this, view_delegate, false /* Not a suggested app */);
      tile_item->SetParentBackgroundColor(kCardBackgroundColor);
      tile_item->SetBorder(
          views::CreateEmptyBorder(kTopBottomPadding, 0, kTopBottomPadding, 0));
      tile_views_.push_back(tile_item);
      AddChildView(tile_item);
    }
  }
}

SearchResultTileItemListView::~SearchResultTileItemListView() = default;

void SearchResultTileItemListView::OnContainerSelected(
    bool from_bottom,
    bool directional_movement) {
  if (num_results() == 0)
    return;

  // If the user came from below using linear controls (eg, Tab, as opposed to
  // directional controls such as Up), select the right-most result. Otherwise,
  // select the left-most result even if coming from below.
  bool select_last = from_bottom && !directional_movement;
  SetSelectedIndex(select_last ? num_results() - 1 : 0);
}

void SearchResultTileItemListView::NotifyFirstResultYIndex(int y_index) {
  for (size_t i = 0; i < static_cast<size_t>(num_results()); ++i)
    tile_views_[i]->result()->set_distance_from_origin(i + y_index);
}

int SearchResultTileItemListView::GetYSize() {
  return num_results() ? 1 : 0;
}

int SearchResultTileItemListView::DoUpdate() {
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_TILE,
          is_fullscreen_app_list_enabled_ ? kMaxNumSearchResultTiles
                                          : kNumSearchResultTiles);

  if (is_fullscreen_app_list_enabled_) {
    SearchResult::ResultType previous_type = SearchResult::RESULT_UNKNOWN;

    for (size_t i = 0; i < kMaxNumSearchResultTiles; ++i) {
      if (i >= display_results.size()) {
        if (is_play_store_app_search_enabled_)
          separator_views_[i]->SetVisible(false);
        tile_views_[i]->SetSearchResult(nullptr);
        continue;
      }

      SearchResult* item = display_results[i];
      tile_views_[i]->SetSearchResult(item);

      if (is_play_store_app_search_enabled_) {
        if (i > 0 && item->result_type() != previous_type) {
          // Add a separator to separate search results of different types.
          // The strategy here is to only add a separator only if current search
          // result type is different from the previous one. The strategy is
          // based on the assumption that the search results are already
          // separated in groups based on their result types.
          separator_views_[i]->SetVisible(true);
        } else {
          separator_views_[i]->SetVisible(false);
        }
      }

      previous_type = item->result_type();
    }
  } else {
    for (size_t i = 0; i < kNumSearchResultTiles; ++i) {
      SearchResult* item =
          i < display_results.size() ? display_results[i] : nullptr;
      tile_views_[i]->SetSearchResult(item);
    }
  }

  set_container_score(
      display_results.empty() ? 0 : display_results.front()->relevance());

  return display_results.size();
}

void SearchResultTileItemListView::UpdateSelectedIndex(int old_selected,
                                                       int new_selected) {
  if (old_selected >= 0)
    tile_views_[old_selected]->SetSelected(false);

  if (new_selected >= 0) {
    tile_views_[new_selected]->SetSelected(true);
    ScrollRectToVisible(GetLocalBounds());
  }
}

bool SearchResultTileItemListView::OnKeyPressed(const ui::KeyEvent& event) {
  int selection_index = selected_index();
  // Also count the separator when Play Store app search feature is enabled.
  const int child_index = is_play_store_app_search_enabled_
                              ? selection_index * 2 + 1
                              : selection_index;
  if (selection_index >= 0 && child_at(child_index)->OnKeyPressed(event))
    return true;

  int dir = 0;
  bool cursor_at_end_of_searchbox =
      search_box_->GetCursorPosition() == search_box_->text().length();
  const int forward_dir = base::i18n::IsRTL() ? -1 : 1;
  switch (event.key_code()) {
    case ui::VKEY_TAB:
      if (event.IsShiftDown())
        dir = -1;
      else
        dir = 1;
      break;
    case ui::VKEY_LEFT:
      // The left key will not capture the key event when the selection is at
      // the beginning of the list. This means that the text cursor in the
      // search box will be allowed to handle the keypress. This will also
      // ignore the keypress if the user has clicked somewhere in the middle of
      // the searchbox.
      if (cursor_at_end_of_searchbox)
        dir = -forward_dir;
      break;
    case ui::VKEY_RIGHT:
      // Only move right if the search box text cursor is at the end of the
      // text.
      if (cursor_at_end_of_searchbox)
        dir = forward_dir;
      break;
    default:
      break;
  }
  if (dir == 0)
    return false;

  selection_index = selection_index + dir;
  if (IsValidSelectionIndex(selection_index)) {
    SetSelectedIndex(selection_index);
    return true;
  }

  return false;
}

}  // namespace app_list
