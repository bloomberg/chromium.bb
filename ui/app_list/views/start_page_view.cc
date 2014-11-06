// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/start_page_view.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/all_apps_tile_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_tile_item_view.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// Layout constants.
const int kTopMargin = 84;
const int kInstantContainerSpacing = 11;
const int kSearchBoxAndTilesSpacing = 40;

// WebView constants.
const int kWebViewWidth = 500;
const int kWebViewHeight = 105;

// DummySearchBoxView constants.
const int kDummySearchBoxWidth = 480;

// Tile container constants.
const size_t kNumStartPageTiles = 4;
const size_t kNumSearchResultTiles = 5;
const int kTileSpacing = 10;

// A placeholder search box which is sized to fit within the start page view.
class DummySearchBoxView : public SearchBoxView {
 public:
  DummySearchBoxView(SearchBoxViewDelegate* delegate,
                     AppListViewDelegate* view_delegate)
      : SearchBoxView(delegate, view_delegate) {
  }

  ~DummySearchBoxView() override {}

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override {
    gfx::Size size(SearchBoxView::GetPreferredSize());
    size.set_width(kDummySearchBoxWidth);
    return size;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummySearchBoxView);
};

}  // namespace

StartPageView::StartPageView(AppListMainView* app_list_main_view,
                             AppListViewDelegate* view_delegate)
    : app_list_main_view_(app_list_main_view),
      view_delegate_(view_delegate),
      search_box_view_(new DummySearchBoxView(this, view_delegate_)),
      results_view_(
          new SearchResultListView(app_list_main_view, view_delegate)),
      instant_container_(new views::View),
      tiles_container_(new views::View),
      show_state_(SHOW_START_PAGE) {
  // The view containing the start page WebContents and DummySearchBoxView.
  InitInstantContainer();
  AddChildView(instant_container_);

  // The view containing the search results.
  AddChildView(results_view_);

  // The view containing the start page tiles.
  InitTilesContainer();
  AddChildView(tiles_container_);

  SetModel(view_delegate_->GetModel());
}

StartPageView::~StartPageView() {
}

void StartPageView::InitInstantContainer() {
  views::BoxLayout* instant_layout_manager = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kInstantContainerSpacing);
  instant_layout_manager->set_inside_border_insets(
      gfx::Insets(kTopMargin, 0, kSearchBoxAndTilesSpacing, 0));
  instant_layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  instant_layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  instant_container_->SetLayoutManager(instant_layout_manager);

  views::View* web_view = view_delegate_->CreateStartPageWebView(
      gfx::Size(kWebViewWidth, kWebViewHeight));
  if (web_view)
    instant_container_->AddChildView(web_view);

  // TODO(calamity): This container is needed to horizontally center the search
  // box view. Remove this container once BoxLayout supports CrossAxisAlignment.
  views::View* search_box_container = new views::View();
  views::BoxLayout* layout_manager =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  search_box_container->SetLayoutManager(layout_manager);
  search_box_container->AddChildView(search_box_view_);

  instant_container_->AddChildView(search_box_container);
}

void StartPageView::InitTilesContainer() {
  views::BoxLayout* tiles_layout_manager =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, kTileSpacing);
  tiles_layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  tiles_container_->SetLayoutManager(tiles_layout_manager);

  // Add SearchResultTileItemViews to the container.
  for (size_t i = 0; i < std::max(kNumStartPageTiles, kNumSearchResultTiles);
       ++i) {
    SearchResultTileItemView* tile_item = new SearchResultTileItemView();
    tiles_container_->AddChildView(tile_item);
    search_result_tile_views_.push_back(tile_item);
  }

  // Also add a special "all apps" button to the end of the container.
  all_apps_button_ = new AllAppsTileItemView(
      app_list_main_view_->contents_view(),
      view_delegate_->GetModel()->top_level_item_list());
  all_apps_button_->UpdateIcon();
  tiles_container_->AddChildView(all_apps_button_);
}

void StartPageView::SetModel(AppListModel* model) {
  DCHECK(model);
  SetResults(model->results());
  results_view_->SetResults(model->results());
  Reset();
}

void StartPageView::Reset() {
  SetShowState(SHOW_START_PAGE);
  Update();
}

void StartPageView::ShowSearchResults() {
  SetShowState(SHOW_SEARCH_RESULTS);
  Update();
}

void StartPageView::SetShowState(ShowState show_state) {
  instant_container_->SetVisible(show_state == SHOW_START_PAGE);
  results_view_->SetVisible(show_state == SHOW_SEARCH_RESULTS);

  // This can be called when the app list is closing (widget is invisible). In
  // that case, do not steal focus from other elements.
  if (show_state == SHOW_START_PAGE && GetWidget() && GetWidget()->IsVisible())
    search_box_view_->search_box()->RequestFocus();

  if (show_state_ == show_state)
    return;

  show_state_ = show_state;

  if (show_state_ == SHOW_START_PAGE)
    search_box_view_->ClearSearch();

  results_view_->UpdateAutoLaunchState();
  if (show_state == SHOW_SEARCH_RESULTS)
    results_view_->SetSelectedIndex(0);
}

bool StartPageView::IsShowingSearchResults() const {
  return show_state_ == SHOW_SEARCH_RESULTS;
}

void StartPageView::UpdateForTesting() {
  Update();
}

TileItemView* StartPageView::all_apps_button() const {
  return all_apps_button_;
}

bool StartPageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (show_state_ == SHOW_SEARCH_RESULTS)
    return results_view_->OnKeyPressed(event);

  return false;
}

void StartPageView::Layout() {
  // Instant and search results take up the height of the instant container.
  gfx::Rect bounds(GetContentsBounds());
  bounds.set_height(instant_container_->GetHeightForWidth(bounds.width()));
  instant_container_->SetBoundsRect(bounds);
  results_view_->SetBoundsRect(bounds);

  // Tiles begin where the instant container ends.
  bounds.set_y(bounds.bottom());
  bounds.set_height(tiles_container_->GetHeightForWidth(bounds.width()));
  tiles_container_->SetBoundsRect(bounds);
}

void StartPageView::Update() {
  size_t max_tiles = show_state_ == SHOW_START_PAGE ? kNumStartPageTiles
                                                    : kNumSearchResultTiles;
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_TILE, max_tiles);
  // Update the tile item results.
  for (size_t i = 0; i < search_result_tile_views_.size(); ++i) {
    SearchResult* item = NULL;
    if (i < display_results.size())
      item = display_results[i];
    search_result_tile_views_[i]->SetSearchResult(item);
  }

  // Show or hide the all apps button (depending on the current show state).
  all_apps_button_->SetVisible(show_state_ == SHOW_START_PAGE);

  tiles_container_->Layout();
  Layout();
}

void StartPageView::QueryChanged(SearchBoxView* sender) {
  // Forward the search terms on to the real search box and clear the dummy
  // search box.
  app_list_main_view_->OnStartPageSearchTextfieldChanged(
      sender->search_box()->text());
  sender->search_box()->SetText(base::string16());
}

}  // namespace app_list
