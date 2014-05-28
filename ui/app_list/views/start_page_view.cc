// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/start_page_view.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

const int kTopMargin = 30;

const int kWebViewWidth = 200;
const int kWebViewHeight = 105;

const int kInstantContainerSpacing = 20;
const int kBarPlaceholderWidth = 490;
const int kBarPlaceholderHeight = 30;

const size_t kNumStartPageTiles = 5;
const int kTileSpacing = 10;

// A button that is the placeholder for the search bar in the start page view.
class BarPlaceholderButton : public views::CustomButton {
 public:
  explicit BarPlaceholderButton(views::ButtonListener* listener)
      : views::CustomButton(listener) {}

  virtual ~BarPlaceholderButton() {}

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(kBarPlaceholderWidth, kBarPlaceholderHeight);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    PaintButton(
        canvas,
        state() == STATE_HOVERED ? kPagerHoverColor : kPagerNormalColor);
  }

 private:
  // Paints a rectangular button.
  void PaintButton(gfx::Canvas* canvas, SkColor base_color) {
    gfx::Rect rect(GetContentsBounds());
    rect.ClampToCenteredSize(
        gfx::Size(kBarPlaceholderWidth, kBarPlaceholderHeight));

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(base_color);
    canvas->DrawRect(rect, paint);
  }

  DISALLOW_COPY_AND_ASSIGN(BarPlaceholderButton);
};

}  // namespace

StartPageView::StartPageView(AppListMainView* app_list_main_view,
                             AppListViewDelegate* view_delegate)
    : app_list_main_view_(app_list_main_view),
      model_(NULL),
      view_delegate_(view_delegate),
      instant_container_(new views::View),
      tiles_container_(new views::View) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, kTopMargin, kInstantContainerSpacing));

  // The view containing the start page WebContents and the BarPlaceholder.
  AddChildView(instant_container_);
  views::BoxLayout* instant_layout_manager = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kInstantContainerSpacing);
  instant_layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  instant_container_->SetLayoutManager(instant_layout_manager);

  content::WebContents* start_page_web_contents =
      view_delegate->GetStartPageContents();
  views::WebView* web_view = new views::WebView(
      start_page_web_contents ? start_page_web_contents->GetBrowserContext()
                              : NULL);
  web_view->SetPreferredSize(gfx::Size(kWebViewWidth, kWebViewHeight));
  web_view->SetWebContents(start_page_web_contents);

  instant_container_->AddChildView(web_view);
  instant_container_->AddChildView(new BarPlaceholderButton(this));

  // The view containing the start page tiles.
  AddChildView(tiles_container_);
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

  SetModel(view_delegate_->GetModel());
  view_delegate_->AddObserver(this);
}

StartPageView::~StartPageView() {
  view_delegate_->RemoveObserver(this);
}

void StartPageView::SetModel(AppListModel* model) {
  DCHECK(model);
  if (model_)
    model_->RemoveObserver(this);
  model_ = model;
  model_->AddObserver(this);
  Reset();
}

void StartPageView::Reset() {
  instant_container_->SetVisible(true);
  if (!model_ || !model_->top_level_item_list())
    return;

  for (size_t i = 0; i < kNumStartPageTiles; ++i) {
    AppListItem* item = NULL;
    if (i < model_->top_level_item_list()->item_count())
      item = model_->top_level_item_list()->item_at(i);
    tile_views_[i]->SetAppListItem(item);
  }

  Layout();
}

void StartPageView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  app_list_main_view_->OnStartPageSearchButtonPressed();
  instant_container_->SetVisible(false);
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
