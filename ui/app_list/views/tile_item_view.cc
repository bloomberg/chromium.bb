// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/tile_item_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

const int kTileSize = 90;
const int kIconTitleSpacing = 6;

}  // namespace

namespace app_list {

TileItemView::TileItemView()
    : views::CustomButton(this),
      parent_background_color_(SK_ColorTRANSPARENT),
      icon_(new views::ImageView),
      title_(new views::Label),
      selected_(false) {
  views::BoxLayout* layout_manager = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kIconTitleSpacing);
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout_manager);

  icon_->SetImageSize(gfx::Size(kTileIconSize, kTileIconSize));
  // Prevent the icon view from interfering with our mouse events.
  icon_->set_interactive(false);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(kGridTitleColor);
  title_->SetFontList(rb.GetFontList(kItemTextFontStyle));
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetHandlesTooltips(false);

  AddChildView(icon_);
  AddChildView(title_);
}

TileItemView::~TileItemView() {
}

void TileItemView::SetSelected(bool selected) {
  if (selected == selected_)
    return;

  selected_ = selected;
  UpdateBackgroundColor();

  if (selected)
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
}

void TileItemView::SetParentBackgroundColor(SkColor color) {
  parent_background_color_ = color;
  UpdateBackgroundColor();
}

void TileItemView::SetIcon(const gfx::ImageSkia& icon) {
  icon_->SetImage(icon);
}

void TileItemView::SetTitle(const base::string16& title) {
  title_->SetText(title);
  SetAccessibleName(title);
}

void TileItemView::StateChanged() {
  UpdateBackgroundColor();
}

void TileItemView::UpdateBackgroundColor() {
  views::Background* background = nullptr;
  SkColor background_color = parent_background_color_;

  if (selected_) {
    background_color = kSelectedColor;
    background = views::Background::CreateSolidBackground(background_color);
  } else if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    background_color = kHighlightedColor;
    background = views::Background::CreateSolidBackground(background_color);
  }

  // Tell the label what color it will be drawn onto. It will use whether the
  // background color is opaque or transparent to decide whether to use subpixel
  // rendering. Does not actually set the label's background color.
  title_->SetBackgroundColor(background_color);

  set_background(background);
  SchedulePaint();
}

gfx::Size TileItemView::GetPreferredSize() const {
  return gfx::Size(kTileSize, kTileSize);
}

bool TileItemView::GetTooltipText(const gfx::Point& p,
                                  base::string16* tooltip) const {
  // Use the label to generate a tooltip, so that it will consider its text
  // truncation in making the tooltip. We do not want the label itself to have a
  // tooltip, so we only temporarily enable it to get the tooltip text from the
  // label, then disable it again.
  title_->SetHandlesTooltips(true);
  bool handled = title_->GetTooltipText(p, tooltip);
  title_->SetHandlesTooltips(false);
  return handled;
}

}  // namespace app_list
