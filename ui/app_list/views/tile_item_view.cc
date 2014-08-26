// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/tile_item_view.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

const int kTileSize = 90;
const int kTileHorizontalPadding = 10;

}  // namespace

namespace app_list {

TileItemView::TileItemView()
    : views::CustomButton(this),
      item_(NULL),
      icon_(new views::ImageView),
      title_(new views::Label) {
  views::BoxLayout* layout_manager = new views::BoxLayout(
      views::BoxLayout::kVertical, kTileHorizontalPadding, 0, 0);
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout_manager);

  icon_->SetImageSize(gfx::Size(kTileIconSize, kTileIconSize));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(kGridTitleColor);
  title_->set_background(views::Background::CreateSolidBackground(
      app_list::kContentsBackgroundColor));
  title_->SetFontList(rb.GetFontList(kItemTextFontStyle));
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  // When |item_| is NULL, the tile is invisible. Calling SetSearchResult with a
  // non-NULL item makes the tile visible.
  SetVisible(false);

  AddChildView(icon_);
  AddChildView(title_);
}

TileItemView::~TileItemView() {
  if (item_)
    item_->RemoveObserver(this);
}

void TileItemView::SetSearchResult(SearchResult* item) {
  SetVisible(item != NULL);

  SearchResult* old_item = item_;
  if (old_item)
    old_item->RemoveObserver(this);

  item_ = item;

  if (!item)
    return;

  item_->AddObserver(this);

  title_->SetText(item_->title());

  // Only refresh the icon if it's different from the old one. This prevents
  // flickering.
  if (old_item == NULL ||
      !item->icon().BackedBySameObjectAs(old_item->icon())) {
    OnIconChanged();
  }
}

gfx::Size TileItemView::GetPreferredSize() const {
  return gfx::Size(kTileSize, kTileSize);
}

void TileItemView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  item_->Open(event.flags());
}

void TileItemView::OnIconChanged() {
  icon_->SetImage(item_->icon());
}

void TileItemView::OnResultDestroying() {
  if (item_)
    item_->RemoveObserver(this);
  item_ = NULL;
}

}  // namespace app_list
