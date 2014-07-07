// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/start_page_view.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
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
const int kTopMargin = 30;
const int kInstantContainerSpacing = 20;

// WebView constants.
const int kWebViewWidth = 500;
const int kWebViewHeight = 105;

// DummySearchBoxView constants.
const int kDummySearchBoxWidth = 490;
const int kDummySearchBoxHeight = 40;
const int kDummySearchBoxBorderWidth = 1;
const int kDummySearchBoxBorderBottomWidth = 2;
const int kDummySearchBoxBorderCornerRadius = 2;

// Tile container constants.
const size_t kNumStartPageTiles = 5;
const int kTileSpacing = 10;

// A background that paints a solid white rounded rect with a thin grey border.
class DummySearchBoxBackground : public views::Background {
 public:
  DummySearchBoxBackground() {}
  virtual ~DummySearchBoxBackground() {}

 private:
  // views::Background overrides:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    gfx::Rect bounds = view->GetContentsBounds();

    SkPaint paint;
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(kStartPageBorderColor);
    canvas->DrawRoundRect(bounds, kDummySearchBoxBorderCornerRadius, paint);
    bounds.Inset(kDummySearchBoxBorderWidth,
                 kDummySearchBoxBorderWidth,
                 kDummySearchBoxBorderWidth,
                 kDummySearchBoxBorderBottomWidth);
    paint.setColor(SK_ColorWHITE);
    canvas->DrawRoundRect(bounds, kDummySearchBoxBorderCornerRadius, paint);
  }

  DISALLOW_COPY_AND_ASSIGN(DummySearchBoxBackground);
};

// A placeholder search box which is sized to fit within the start page view.
class DummySearchBoxView : public SearchBoxView {
 public:
  DummySearchBoxView(SearchBoxViewDelegate* delegate,
                     AppListViewDelegate* view_delegate)
      : SearchBoxView(delegate, view_delegate) {
    set_background(new DummySearchBoxBackground());
  }

  virtual ~DummySearchBoxView() {}

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(kDummySearchBoxWidth, kDummySearchBoxHeight);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummySearchBoxView);
};

}  // namespace

StartPageView::StartPageView(AppListMainView* app_list_main_view,
                             AppListViewDelegate* view_delegate)
    : app_list_main_view_(app_list_main_view),
      model_(NULL),
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
  view_delegate_->AddObserver(this);
}

StartPageView::~StartPageView() {
  view_delegate_->RemoveObserver(this);
  if (model_)
    model_->RemoveObserver(this);
}

void StartPageView::InitInstantContainer() {
  views::BoxLayout* instant_layout_manager = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kInstantContainerSpacing);
  instant_layout_manager->set_inside_border_insets(
      gfx::Insets(kTopMargin, 0, kInstantContainerSpacing, 0));
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
  for (size_t i = 0; i < kNumStartPageTiles; ++i) {
    TileItemView* tile_item = new TileItemView();
    tiles_container_->AddChildView(tile_item);
    tile_views_.push_back(tile_item);
  }
}

void StartPageView::SetModel(AppListModel* model) {
  DCHECK(model);
  if (model_)
    model_->RemoveObserver(this);
  model_ = model;
  model_->AddObserver(this);
  results_view_->SetResults(model_->results());
  Reset();
}

void StartPageView::Reset() {
  SetShowState(SHOW_START_PAGE);
  if (!model_ || !model_->top_level_item_list())
    return;

  for (size_t i = 0; i < kNumStartPageTiles; ++i) {
    AppListItem* item = NULL;
    if (i < model_->top_level_item_list()->item_count())
      item = model_->top_level_item_list()->item_at(i);
    tile_views_[i]->SetAppListItem(item);
  }
}

void StartPageView::ShowSearchResults() {
  SetShowState(SHOW_SEARCH_RESULTS);
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

  results_view_->UpdateAutoLaunchState();
  if (show_state == SHOW_SEARCH_RESULTS)
    results_view_->SetSelectedIndex(0);
}

bool StartPageView::IsShowingSearchResults() const {
  return show_state_ == SHOW_SEARCH_RESULTS;
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

void StartPageView::QueryChanged(SearchBoxView* sender) {
  // Forward the search terms on to the real search box and clear the dummy
  // search box.
  app_list_main_view_->OnStartPageSearchTextfieldChanged(
      sender->search_box()->text());
  sender->search_box()->SetText(base::string16());
}

void StartPageView::OnProfilesChanged() {
  SetModel(view_delegate_->GetModel());
}

void StartPageView::OnAppListModelStatusChanged() {
  Reset();
}

void StartPageView::OnAppListItemAdded(AppListItem* item) {
  Reset();
}

void StartPageView::OnAppListItemDeleted() {
  Reset();
}

void StartPageView::OnAppListItemUpdated(AppListItem* item) {
  Reset();
}

}  // namespace app_list
