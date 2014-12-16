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
#include "ui/app_list/views/contents_view.h"
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
const int kStartPageSearchBoxWidth = 480;

// WebView constants.
const int kWebViewWidth = 500;
const int kWebViewHeight = 105;

// Tile container constants.
const size_t kNumStartPageTiles = 4;
const int kTileSpacing = 10;

// An invisible placeholder view which fills the space for the search box view
// in a box layout. The search box view itself is a child of the AppListView
// (because it is visible on many different pages).
class SearchBoxSpacerView : public views::View {
 public:
  explicit SearchBoxSpacerView(const gfx::Size& search_box_size)
      : size_(kStartPageSearchBoxWidth, search_box_size.height()) {}

  ~SearchBoxSpacerView() override {}

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override { return size_; }

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxSpacerView);
};

}  // namespace

StartPageView::StartPageView(AppListMainView* app_list_main_view,
                             AppListViewDelegate* view_delegate)
    : app_list_main_view_(app_list_main_view),
      view_delegate_(view_delegate),
      search_box_spacer_view_(new SearchBoxSpacerView(
          app_list_main_view->search_box_view()->GetPreferredSize())),
      instant_container_(new views::View),
      tiles_container_(new views::View) {
  // The view containing the start page WebContents and SearchBoxSpacerView.
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
  if (web_view) {
    web_view->SetFocusable(false);
    instant_container_->AddChildView(web_view);
  }

  instant_container_->AddChildView(search_box_spacer_view_);
}

void StartPageView::InitTilesContainer() {
  views::BoxLayout* tiles_layout_manager =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, kTileSpacing);
  tiles_layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  tiles_container_->SetLayoutManager(tiles_layout_manager);
  tiles_container_->set_background(
      views::Background::CreateSolidBackground(kLabelBackgroundColor));

  // Add SearchResultTileItemViews to the container.
  for (size_t i = 0; i < kNumStartPageTiles; ++i) {
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

void StartPageView::Reset() {
  Update();
}

void StartPageView::UpdateForTesting() {
  Update();
}

TileItemView* StartPageView::all_apps_button() const {
  return all_apps_button_;
}

void StartPageView::OnShow() {
  DCHECK(app_list_main_view_->contents_view()->ShouldShowCustomPageClickzone());
  UpdateCustomPageClickzoneVisibility();
  ClearSelectedIndex();
}

void StartPageView::OnHide() {
  DCHECK(
      !app_list_main_view_->contents_view()->ShouldShowCustomPageClickzone());
  UpdateCustomPageClickzoneVisibility();
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

bool StartPageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (selected_index() >= 0 &&
      tiles_container_->child_at(selected_index())->OnKeyPressed(event))
    return true;

  int dir = 0;
  switch (event.key_code()) {
    case ui::VKEY_LEFT:
      dir = -1;
      break;
    case ui::VKEY_RIGHT:
      dir = 1;
      break;
    case ui::VKEY_DOWN:
      // Down selects the first tile if nothing is selected.
      if (!IsValidSelectionIndex(selected_index()))
        dir = 1;
      break;
    case ui::VKEY_TAB:
      dir = event.IsShiftDown() ? -1 : 1;
      break;
    default:
      break;
  }

  if (dir == 0)
    return false;

  if (!IsValidSelectionIndex(selected_index())) {
    SetSelectedIndex(dir == -1 ? num_results() - 1 : 0);
    return true;
  }

  int selection_index = selected_index() + dir;
  if (IsValidSelectionIndex(selection_index)) {
    SetSelectedIndex(selection_index);
    return true;
  }

  return false;
}

void StartPageView::OnContainerSelected(bool from_bottom) {
}

gfx::Rect StartPageView::GetSearchBoxBounds() const {
  return search_box_spacer_view_->bounds();
}

void StartPageView::UpdateCustomPageClickzoneVisibility() {
  // This can get called before InitWidgets(), so we cannot guarantee that
  // custom_page_clickzone_ will not be null.
  views::Widget* custom_page_clickzone =
      app_list_main_view_->GetCustomPageClickzone();
  if (!custom_page_clickzone)
    return;

  if (app_list_main_view_->contents_view()->ShouldShowCustomPageClickzone()) {
    custom_page_clickzone->ShowInactive();
    return;
  }

  custom_page_clickzone->Hide();
}

int StartPageView::Update() {
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_RECOMMENDATION, kNumStartPageTiles);

  // Update the tile item results.
  for (size_t i = 0; i < search_result_tile_views_.size(); ++i) {
    SearchResult* item = NULL;
    if (i < display_results.size())
      item = display_results[i];
    search_result_tile_views_[i]->SetSearchResult(item);
  }

  tiles_container_->Layout();
  Layout();
  // Add 1 to the results size to account for the all apps button.
  return display_results.size() + 1;
}

void StartPageView::UpdateSelectedIndex(int old_selected, int new_selected) {
  if (old_selected >= 0)
    GetTileItemView(old_selected)->SetSelected(false);

  if (new_selected >= 0)
    GetTileItemView(new_selected)->SetSelected(true);
}

TileItemView* StartPageView::GetTileItemView(size_t index) {
  DCHECK_GT(kNumStartPageTiles + 1, index);
  if (index == kNumStartPageTiles)
    return all_apps_button_;

  return search_result_tile_views_[index];
}

}  // namespace app_list
