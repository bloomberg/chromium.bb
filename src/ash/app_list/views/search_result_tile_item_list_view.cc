// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_tile_item_list_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Layout constants used when fullscreen app list feature is enabled.
constexpr size_t kMaxNumSearchResultTiles = 6;
constexpr int kItemListVerticalSpacing = 16;
constexpr int kItemListHorizontalSpacing = 12;
constexpr int kBetweenItemSpacing = 8;
constexpr int kSeparatorLeftRightPadding = 4;
constexpr int kSeparatorHeight = 46;
constexpr int kSeparatorTopPadding = 10;

constexpr SkColor kSeparatorColor = SkColorSetA(gfx::kGoogleGrey900, 0x24);

// Returns true if the search result is an installable app.
bool IsResultAnInstallableApp(app_list::SearchResult* result) {
  app_list::SearchResult::ResultType result_type = result->result_type();
  return result_type == ash::SearchResultType::kPlayStoreApp ||
         result_type == ash::SearchResultType::kPlayStoreReinstallApp ||
         result_type == ash::SearchResultType::kInstantApp;
}

}  // namespace

namespace app_list {

SearchResultTileItemListView::SearchResultTileItemListView(
    SearchResultPageView* search_result_page_view,
    views::Textfield* search_box,
    AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate),
      search_result_page_view_(search_result_page_view),
      search_box_(search_box),
      is_play_store_app_search_enabled_(
          app_list_features::IsPlayStoreAppSearchEnabled()),
      is_app_reinstall_recommendation_enabled_(
          app_list_features::IsAppReinstallZeroStateEnabled()) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      gfx::Insets(kItemListVerticalSpacing, kItemListHorizontalSpacing),
      kBetweenItemSpacing));
  for (size_t i = 0; i < kMaxNumSearchResultTiles; ++i) {
    if (is_app_reinstall_recommendation_enabled_ ||
        is_play_store_app_search_enabled_) {
      views::Separator* separator = new views::Separator;
      separator->SetVisible(false);
      separator->SetBorder(views::CreateEmptyBorder(
          kSeparatorTopPadding, kSeparatorLeftRightPadding,
          AppListConfig::instance().search_tile_height() - kSeparatorHeight,
          kSeparatorLeftRightPadding));
      separator->SetColor(kSeparatorColor);

      separator_views_.push_back(separator);
      AddChildView(separator);
    }

    SearchResultTileItemView* tile_item = new SearchResultTileItemView(
        view_delegate, nullptr /* pagination model */,
        false /* show_in_apps_page */);
    tile_item->SetIndexInItemListView(i);
    tile_item->SetParentBackgroundColor(
        AppListConfig::instance().card_background_color());
    tile_views_.push_back(tile_item);
    AddChildView(tile_item);
    AddObservedResultView(tile_item);
  }
}

SearchResultTileItemListView::~SearchResultTileItemListView() = default;

void SearchResultTileItemListView::NotifyFirstResultYIndex(int y_index) {
  for (size_t i = 0; i < static_cast<size_t>(num_results()); ++i)
    tile_views_[i]->result()->set_distance_from_origin(i + y_index);
}

int SearchResultTileItemListView::GetYSize() {
  return num_results() ? 1 : 0;
}

SearchResultBaseView* SearchResultTileItemListView::GetFirstResultView() {
  DCHECK(!tile_views_.empty());
  return num_results() <= 0 ? nullptr : tile_views_[0];
}

int SearchResultTileItemListView::DoUpdate() {
  std::vector<SearchResult*> display_results = GetDisplayResults();

  std::set<std::string> result_id_removed, result_id_added;
  bool is_result_an_installable_app = false;
  bool is_previous_result_installable_app = false;
  for (size_t i = 0; i < kMaxNumSearchResultTiles; ++i) {
    // If the current result at i exists, wants to be notified and is a
    // different id, notify it that it is being hidden.
    SearchResult* current_result = tile_views_[i]->result();
    if (current_result != nullptr) {
      result_id_removed.insert(current_result->id());
    }

    if (i >= display_results.size()) {
      if (is_app_reinstall_recommendation_enabled_ ||
          is_play_store_app_search_enabled_) {
        separator_views_[i]->SetVisible(false);
      }

      tile_views_[i]->SetResult(nullptr);
      continue;
    }

    SearchResult* item = display_results[i];

    tile_views_[i]->SetResult(item);
    result_id_added.insert(item->id());
    is_result_an_installable_app = IsResultAnInstallableApp(item);

    if (is_play_store_app_search_enabled_ ||
        is_app_reinstall_recommendation_enabled_) {
      if (i > 0 && (is_result_an_installable_app !=
                    is_previous_result_installable_app)) {
        // Add a separator between installed apps and installable apps.
        // This assumes the search results are already separated in groups for
        // installed and installable apps.
        separator_views_[i]->SetVisible(true);
      } else {
        separator_views_[i]->SetVisible(false);
      }
    }

    is_previous_result_installable_app = is_result_an_installable_app;
  }

  // notify visibility changes, if needed.
  std::set<std::string> actual_added_ids =
      base::STLSetDifference<std::set<std::string>>(result_id_added,
                                                    result_id_removed);

  for (const std::string& added_id : actual_added_ids) {
    SearchResult* added =
        view_delegate()->GetSearchModel()->FindSearchResult(added_id);
    if (added != nullptr && added->notify_visibility_change()) {
      view_delegate()->OnSearchResultVisibilityChanged(added->id(), shown());
    }
  }
  if (shown() != false) {
    std::set<std::string> actual_removed_ids =
        base::STLSetDifference<std::set<std::string>>(result_id_removed,
                                                      result_id_added);
    // we only notify removed items if we're in the middle of showing.
    for (const std::string& removed_id : actual_removed_ids) {
      SearchResult* removed =
          view_delegate()->GetSearchModel()->FindSearchResult(removed_id);
      if (removed != nullptr && removed->notify_visibility_change()) {
        view_delegate()->OnSearchResultVisibilityChanged(removed->id(),
                                                         false /*=shown*/);
      }
    }
  }

  set_container_score(
      display_results.empty() ? 0 : display_results.front()->display_score());

  return display_results.size();
}

std::vector<SearchResult*> SearchResultTileItemListView::GetDisplayResults() {
  base::string16 raw_query = search_box_->text();
  base::string16 query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);

  // We ask for kMaxNumSearchResultTiles total results, and we prefer reinstall
  // candidates if appropriate. we fetch |reinstall_results| first, and
  // front-fill the rest from the regular result types.
  auto reinstall_filter =
      base::BindRepeating([](const SearchResult& r) -> bool {
        return r.display_type() ==
                   ash::SearchResultDisplayType::kRecommendation &&
               r.result_type() == ash::SearchResultType::kPlayStoreReinstallApp;
      });
  std::vector<SearchResult*> reinstall_results =
      is_app_reinstall_recommendation_enabled_ && query.empty()
          ? SearchModel::FilterSearchResultsByFunction(
                results(), reinstall_filter, kMaxNumSearchResultTiles)
          : std::vector<SearchResult*>();

  SearchResult::DisplayType display_type =
      app_list_features::IsZeroStateSuggestionsEnabled()
          ? (query.empty() ? ash::SearchResultDisplayType::kRecommendation
                           : ash::SearchResultDisplayType::kTile)
          : ash::SearchResultDisplayType::kTile;
  size_t display_num = kMaxNumSearchResultTiles - reinstall_results.size();

  // Do not display the continue reading app in the search result list.
  auto non_reinstall_filter = base::BindRepeating(
      [](const SearchResult::DisplayType& display_type,
         const SearchResult& r) -> bool {
        return r.display_type() == display_type &&
               r.result_type() !=
                   ash::SearchResultType::kPlayStoreReinstallApp &&
               r.id() != app_list::kInternalAppIdContinueReading;
      },
      display_type);
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), non_reinstall_filter, display_num);

  // Append the reinstalls to the display results.
  display_results.insert(display_results.end(), reinstall_results.begin(),
                         reinstall_results.end());
  return display_results;
}

bool SearchResultTileItemListView::OnKeyPressed(const ui::KeyEvent& event) {
  // Let the FocusManager handle Left/Right keys.
  if (!IsUnhandledUpDownKeyEvent(event))
    return false;

  views::View* next_focusable_view = nullptr;

  // Since search result tile item views have horizontal layout, hitting
  // up/down when one of them is focused moves focus to the previous/next
  // search result container.
  if (event.key_code() == ui::VKEY_UP) {
    next_focusable_view = GetFocusManager()->GetNextFocusableView(
        tile_views_.front(), GetWidget(), true, false);
    if (!search_result_page_view_->Contains(next_focusable_view)) {
      // Focus should be moved to search box when it is moved outside search
      // result page view.
      search_box_->RequestFocus();
      return true;
    }
  } else {
    DCHECK_EQ(event.key_code(), ui::VKEY_DOWN);
    next_focusable_view = GetFocusManager()->GetNextFocusableView(
        tile_views_.back(), GetWidget(), false, false);
  }

  if (next_focusable_view) {
    next_focusable_view->RequestFocus();
    return true;
  }

  // Return false to let FocusManager to handle default focus move by key
  // events.
  return false;
}

const char* SearchResultTileItemListView::GetClassName() const {
  return "SearchResultTileItemListView";
}

void SearchResultTileItemListView::OnShownChanged() {
  SearchResultContainerView::OnShownChanged();
  for (const auto* tile_view : tile_views_) {
    SearchResult* result = tile_view->result();
    if (result == nullptr) {
      continue;
    }
    if (result->notify_visibility_change()) {
      view_delegate()->OnSearchResultVisibilityChanged(result->id(), shown());
    }
  }
}

void SearchResultTileItemListView::VisibilityChanged(View* starting_from,
                                                     bool is_visible) {
  SearchResultContainerView::VisibilityChanged(starting_from, is_visible);
  // We only do this work when is_visible is false, since this is how we
  // receive the event. We filter and only run when shown.
  if (is_visible && shown()) {
    return;
  }
  for (const auto* tile_view : tile_views_) {
    SearchResult* result = tile_view->result();
    if (result == nullptr) {
      continue;
    }
    if (result->notify_visibility_change()) {
      view_delegate()->OnSearchResultVisibilityChanged(result->id(),
                                                       false /*=visible*/);
    }
  }
}

}  // namespace app_list
