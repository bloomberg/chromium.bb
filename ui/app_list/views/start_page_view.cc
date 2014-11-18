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
      instant_container_(new views::View),
      tiles_container_(new views::View) {
  // The view containing the start page WebContents and DummySearchBoxView.
  InitInstantContainer();
  AddChildView(instant_container_);

  // The view containing the start page tiles.
  InitTilesContainer();
  AddChildView(tiles_container_);

  SetResults(view_delegate_->GetModel()->results());
  Reset();
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
  for (size_t i = 0; i < kNumStartPageTiles; ++i) {
    SearchResultTileItemView* tile_item = new SearchResultTileItemView();
    tiles_container_->AddChildView(tile_item);
    tile_item->SetTitleBackgroundColor(kLabelBackgroundColor);
    search_result_tile_views_.push_back(tile_item);
  }

  // Also add a special "all apps" button to the end of the container.
  all_apps_button_ = new AllAppsTileItemView(
      app_list_main_view_->contents_view(),
      view_delegate_->GetModel()->top_level_item_list());
  all_apps_button_->UpdateIcon();
  all_apps_button_->SetTitleBackgroundColor(kLabelBackgroundColor);
  tiles_container_->AddChildView(all_apps_button_);
}

void StartPageView::Reset() {
  // This can be called when the app list is closing (widget is invisible). In
  // that case, do not steal focus from other elements.
  if (GetWidget() && GetWidget()->IsVisible())
    search_box_view_->search_box()->RequestFocus();

  search_box_view_->ClearSearch();

  Update();
}

void StartPageView::UpdateForTesting() {
  Update();
}

TileItemView* StartPageView::all_apps_button() const {
  return all_apps_button_;
}

void StartPageView::Layout() {
  gfx::Rect bounds(GetContentsBounds());
  bounds.set_height(instant_container_->GetHeightForWidth(bounds.width()));
  instant_container_->SetBoundsRect(bounds);

  // Tiles begin where the instant container ends.
  bounds.set_y(bounds.bottom());
  bounds.set_height(tiles_container_->GetHeightForWidth(bounds.width()));
  tiles_container_->SetBoundsRect(bounds);
}

void StartPageView::OnContainerSelected(bool from_bottom) {
}

int StartPageView::Update() {
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_TILE, kNumStartPageTiles);

  // Update the tile item results.
  for (size_t i = 0; i < search_result_tile_views_.size(); ++i) {
    SearchResult* item = NULL;
    if (i < display_results.size())
      item = display_results[i];
    search_result_tile_views_[i]->SetSearchResult(item);
  }

  tiles_container_->Layout();
  Layout();
  return display_results.size();
}

void StartPageView::UpdateSelectedIndex(int old_selected, int new_selected) {
}

void StartPageView::QueryChanged(SearchBoxView* sender) {
  // Forward the search terms on to the real search box and clear the dummy
  // search box.
  app_list_main_view_->OnStartPageSearchTextfieldChanged(
      sender->search_box()->text());
  sender->search_box()->SetText(base::string16());
}

}  // namespace app_list
