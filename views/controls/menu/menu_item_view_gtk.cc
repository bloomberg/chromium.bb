// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_item_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/favicon_size.h"
#include "app/resource_bundle.h"
#include "grit/app_resources.h"
#include "views/controls/button/text_button.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

// Background color when the menu item is selected.
static const SkColor kSelectedBackgroundColor = SkColorSetRGB(246, 249, 253);

gfx::Size MenuItemView::GetPreferredSize() {
  const gfx::Font& font = MenuConfig::instance().font;
  // TODO(sky): this is a workaround until I figure out why font.height()
  // isn't returning the right thing. We really only want to include
  // kFavIconSize if we're showing icons.
  int content_height = std::max(kFavIconSize, font.height());
  return gfx::Size(
      font.GetStringWidth(title_) + label_start_ + item_right_margin_,
      content_height + GetBottomMargin() + GetTopMargin());
}

void MenuItemView::Paint(gfx::Canvas* canvas, bool for_drag) {
  const MenuConfig& config = MenuConfig::instance();
  bool render_selection =
      (!for_drag && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this));

  int icon_x = config.item_left_margin;
  int top_margin = GetTopMargin();
  int bottom_margin = GetBottomMargin();
  int icon_y = top_margin + (height() - config.item_top_margin -
                             bottom_margin - config.check_height) / 2;
  int icon_height = config.check_height;
  int available_height = height() - top_margin - bottom_margin;

  // Render the background. As MenuScrollViewContainer draws the background, we
  // only need the background when we want it to look different, as when we're
  // selected.
  if (render_selection)
    canvas->drawColor(kSelectedBackgroundColor, SkXfermode::kSrc_Mode);

  // Render the check.
  if (type_ == CHECKBOX && GetDelegate()->IsItemChecked(GetCommand())) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap* check = rb.GetBitmapNamed(IDR_MENU_CHECK);
    // Don't use config.check_width here as it's padded to force more padding.
    gfx::Rect check_bounds(icon_x, icon_y, check->width(), icon_height);
    AdjustBoundsForRTLUI(&check_bounds);
    canvas->DrawBitmapInt(*check, check_bounds.x(), check_bounds.y());
  }

  // Render the foreground.
  SkColor fg_color =
      IsEnabled() ? TextButton::kEnabledColor : TextButton::kDisabledColor;
  int width = this->width() - item_right_margin_ - label_start_;
  const gfx::Font& font = MenuConfig::instance().font;
  gfx::Rect text_bounds(label_start_, top_margin +
                        (available_height - font.height()) / 2, width,
                        font.height());
  text_bounds.set_x(MirroredLeftPointForRect(text_bounds));
  canvas->DrawStringInt(GetTitle(), font, fg_color,
                        text_bounds.x(), text_bounds.y(), text_bounds.width(),
                        text_bounds.height(),
                        GetRootMenuItem()->GetDrawStringFlags());

  // Render the icon.
  if (icon_.width() > 0) {
    gfx::Rect icon_bounds(config.item_left_margin,
                          top_margin + (height() - top_margin -
                          bottom_margin - icon_.height()) / 2,
                          icon_.width(),
                          icon_.height());
    icon_bounds.set_x(MirroredLeftPointForRect(icon_bounds));
    canvas->DrawBitmapInt(icon_, icon_bounds.x(), icon_bounds.y());
  }

  // Render the submenu indicator (arrow).
  if (HasSubmenu()) {
    gfx::Rect arrow_bounds(this->width() - item_right_margin_ +
                           config.label_to_arrow_padding,
                           top_margin + (available_height -
                                         config.arrow_width) / 2,
                           config.arrow_width, height());
    AdjustBoundsForRTLUI(&arrow_bounds);
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_MENU_ARROW),
                          arrow_bounds.x(), arrow_bounds.y());
  }
}

}  // namespace views
