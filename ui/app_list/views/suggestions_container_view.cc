// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/suggestions_container_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/views/all_apps_tile_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_result_tile_item_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/grid_layout.h"

namespace app_list {

namespace {

constexpr int kTileSpacing = 7;
constexpr int kNumTilesCols = 5;
constexpr int kTilesHorizontalMarginLeft = 145;

}  // namespace

SuggestionsContainerView::SuggestionsContainerView(
    ContentsView* contents_view,
    AllAppsTileItemView* all_apps_button)
    : contents_view_(contents_view),
      all_apps_button_(all_apps_button),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  DCHECK(contents_view);
  view_delegate_ = contents_view_->app_list_main_view()->view_delegate();
  SetBackground(views::CreateSolidBackground(kLabelBackgroundColor));
  if (all_apps_button_) {
    all_apps_button_->SetHoverStyle(TileItemView::HOVER_STYLE_ANIMATE_SHADOW);
    all_apps_button_->SetParentBackgroundColor(kLabelBackgroundColor);
  }

  CreateAppsGrid(is_fullscreen_app_list_enabled_ ? kNumStartPageTilesFullscreen
                                                 : kNumStartPageTiles);
}

SuggestionsContainerView::~SuggestionsContainerView() = default;

TileItemView* SuggestionsContainerView::GetTileItemView(int index) {
  DCHECK_GT(num_results(), index);
  if (all_apps_button_ && index == num_results() - 1)
    return all_apps_button_;

  return search_result_tile_views_[index];
}

int SuggestionsContainerView::DoUpdate() {
  // Ignore updates and disable buttons when suggestions container view is not
  // shown. For bubble launcher, that is not on STATE_START state; for
  // fullscreen launcher, that is not on STATE_START and STATE_APPS state.
  const AppListModel::State state = contents_view_->GetActiveState();
  if (state != AppListModel::STATE_START &&
      (!is_fullscreen_app_list_enabled_ || state != AppListModel::STATE_APPS)) {
    for (auto* view : search_result_tile_views_)
      view->SetEnabled(false);

    return num_results();
  }

  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_RECOMMENDATION,
          is_fullscreen_app_list_enabled_ ? kNumStartPageTilesFullscreen
                                          : kNumStartPageTiles);
  if (display_results.size() != search_result_tile_views_.size()) {
    // We should recreate the grid layout in this case.
    for (size_t i = 0; i < search_result_tile_views_.size(); ++i)
      delete search_result_tile_views_[i];
    search_result_tile_views_.clear();
    if (all_apps_button_)
      RemoveChildView(all_apps_button_);

    CreateAppsGrid(is_fullscreen_app_list_enabled_
                       ? kNumStartPageTilesFullscreen
                       : std::min(kNumStartPageTiles, display_results.size()));
  }

  // Update the tile item results.
  for (size_t i = 0; i < search_result_tile_views_.size(); ++i) {
    SearchResult* item = nullptr;
    if (i < display_results.size())
      item = display_results[i];
    search_result_tile_views_[i]->SetSearchResult(item);
    search_result_tile_views_[i]->SetEnabled(true);
  }

  parent()->Layout();
  // If |is_fullscreen_app_list_enabled_| is not enabled, add 1 to the results
  // size to account for the all apps button.
  return is_fullscreen_app_list_enabled_ ? display_results.size()
                                         : display_results.size() + 1;
}

void SuggestionsContainerView::UpdateSelectedIndex(int old_selected,
                                                   int new_selected) {
  if (old_selected >= 0 && old_selected < num_results())
    GetTileItemView(old_selected)->SetSelected(false);

  if (new_selected >= 0 && new_selected < num_results())
    GetTileItemView(new_selected)->SetSelected(true);
}

void SuggestionsContainerView::OnContainerSelected(
    bool /*from_bottom*/,
    bool /*directional_movement*/) {
  NOTREACHED();
}

void SuggestionsContainerView::NotifyFirstResultYIndex(int /*y_index*/) {
  NOTREACHED();
}

int SuggestionsContainerView::GetYSize() {
  NOTREACHED();
  return 0;
}

void SuggestionsContainerView::CreateAppsGrid(int apps_num) {
  DCHECK(search_result_tile_views_.empty());
  views::GridLayout* tiles_layout_manager = new views::GridLayout(this);
  SetLayoutManager(tiles_layout_manager);

  views::ColumnSet* column_set = tiles_layout_manager->AddColumnSet(0);
  if (!is_fullscreen_app_list_enabled_)
    column_set->AddPaddingColumn(0, kTilesHorizontalMarginLeft);
  for (int col = 0; col < kNumTilesCols; ++col) {
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(
        0, is_fullscreen_app_list_enabled_ ? kGridTileSpacing : kTileSpacing);
  }

  // Add SearchResultTileItemViews to the container.
  int i = 0;
  search_result_tile_views_.reserve(apps_num);
  const bool is_fullscreen_app_list_enabled =
      features::IsFullscreenAppListEnabled();
  const bool is_play_store_app_search_enabled =
      features::IsPlayStoreAppSearchEnabled();
  for (; i < apps_num; ++i) {
    SearchResultTileItemView* tile_item = new SearchResultTileItemView(
        this, view_delegate_, true, is_fullscreen_app_list_enabled,
        is_play_store_app_search_enabled);
    if (i % kNumTilesCols == 0)
      tiles_layout_manager->StartRow(0, 0);
    tiles_layout_manager->AddView(tile_item);
    AddChildView(tile_item);
    tile_item->SetParentBackgroundColor(kLabelBackgroundColor);
    tile_item->SetHoverStyle(TileItemView::HOVER_STYLE_ANIMATE_SHADOW);
    search_result_tile_views_.emplace_back(tile_item);
  }

  if (all_apps_button_ && !is_fullscreen_app_list_enabled_) {
    all_apps_button_->UpdateIcon();

    // Also add a special "all apps" button to the end of the next row of the
    // container.
    if (i % kNumTilesCols == 0)
      tiles_layout_manager->StartRow(0, 0);

    tiles_layout_manager->AddView(all_apps_button_);
    AddChildView(all_apps_button_);
  }
}

}  // namespace app_list
