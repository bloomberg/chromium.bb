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

const SkColor kTileBackgroundColor = SK_ColorWHITE;
const int kTileColorStripHeight = 2;
const SkAlpha kTileColorStripOpacity = 0X5F;
const int kTileCornerRadius = 2;

}  // namespace

namespace app_list {

// A background for the start page item view which consists of a rounded rect
// with a dominant color strip at the bottom.
class TileItemView::TileItemBackground : public views::Background {
 public:
  TileItemBackground() : strip_color_(SK_ColorBLACK) {}
  virtual ~TileItemBackground() {}

  void set_strip_color(SkColor strip_color) { strip_color_ = strip_color; }

  // Overridden from views::Background:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setFlags(SkPaint::kAntiAlias_Flag);

    // Paint the border.
    paint.setColor(kStartPageBorderColor);
    canvas->DrawRoundRect(view->GetContentsBounds(), kTileCornerRadius, paint);

    // Paint a rectangle for the color strip.
    gfx::Rect color_strip_rect(view->GetContentsBounds());
    color_strip_rect.Inset(1, 1, 1, 1);
    paint.setColor(SkColorSetA(strip_color_, kTileColorStripOpacity));
    canvas->DrawRoundRect(color_strip_rect, kTileCornerRadius, paint);

    // Paint the main background rectangle, leaving part of the color strip
    // unobscured.
    gfx::Rect static_background_rect(color_strip_rect);
    static_background_rect.Inset(0, 0, 0, kTileColorStripHeight);
    paint.setColor(kTileBackgroundColor);
    canvas->DrawRoundRect(static_background_rect, kTileCornerRadius, paint);
  }

 private:
  SkColor strip_color_;

  DISALLOW_COPY_AND_ASSIGN(TileItemBackground);
};

TileItemView::TileItemView()
    : views::CustomButton(this),
      item_(NULL),
      icon_(new views::ImageView),
      title_(new views::Label),
      background_(new TileItemBackground()) {
  set_background(background_);

  views::BoxLayout* layout_manager = new views::BoxLayout(
      views::BoxLayout::kVertical, kTileHorizontalPadding, 0, 0);
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout_manager);

  icon_->SetImageSize(gfx::Size(kTileIconSize, kTileIconSize));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(kGridTitleColor);
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
  background_->set_strip_color(
      color_utils::CalculateKMeanColorOfBitmap(*item_->icon().bitmap()));
  SchedulePaint();
}

void TileItemView::OnResultDestroying() {
  if (item_)
    item_->RemoveObserver(this);
  item_ = NULL;
}

}  // namespace app_list
