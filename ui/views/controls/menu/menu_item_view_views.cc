// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_item_view.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/native_theme/native_theme.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_image_util.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace views {

void MenuItemView::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  const MenuConfig& config = MenuConfig::instance();
  bool render_selection =
      (mode == PB_NORMAL && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this) &&
       (NonIconChildViewsCount() == 0));

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
  if (render_selection) {
    SkColor bg_color = ui::NativeTheme::instance()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
    canvas->DrawColor(bg_color, SkXfermode::kSrc_Mode);
  }

  // Render the check.
  if (type_ == CHECKBOX && GetDelegate()->IsItemChecked(GetCommand())) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* check = rb.GetImageNamed(
        IDR_MENU_CHECK).ToImageSkia();
    // Don't use config.check_width here as it's padded to force more padding.
    gfx::Rect check_bounds(icon_x, icon_y, check->width(), icon_height);
    AdjustBoundsForRTLUI(&check_bounds);
    canvas->DrawImageInt(*check, check_bounds.x(), check_bounds.y());
  } else if (type_ == RADIO) {
    const gfx::ImageSkia* image =
        GetRadioButtonImage(GetDelegate()->IsItemChecked(GetCommand()));
    gfx::Rect radio_bounds(icon_x,
                           top_margin +
                           (height() - top_margin - bottom_margin -
                            image->height()) / 2,
                           image->width(),
                           image->height());
    AdjustBoundsForRTLUI(&radio_bounds);
    canvas->DrawImageInt(*image, radio_bounds.x(), radio_bounds.y());
  }

  // Render the foreground.
  SkColor fg_color = ui::NativeTheme::instance()->GetSystemColor(
      enabled() ? ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor
          : ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor);

  const gfx::Font& font = GetFont();
  int accel_width = parent_menu_item_->GetSubmenu()->max_accelerator_width();
  int width = this->width() - item_right_margin_ - label_start_ - accel_width;
  gfx::Rect text_bounds(label_start_, top_margin, width, available_height);
  text_bounds.set_x(GetMirroredXForRect(text_bounds));
  int flags = GetRootMenuItem()->GetDrawStringFlags() |
              gfx::Canvas::TEXT_VALIGN_MIDDLE;
  if (mode == PB_FOR_DRAG)
    flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;
  canvas->DrawStringInt(title(), font, fg_color,
                        text_bounds.x(), text_bounds.y(), text_bounds.width(),
                        text_bounds.height(), flags);

  PaintAccelerator(canvas);

  // Render the submenu indicator (arrow).
  if (HasSubmenu()) {
    gfx::Rect arrow_bounds(this->width() - config.arrow_width -
                               config.arrow_to_edge_padding,
                           top_margin + (available_height -
                                         config.arrow_width) / 2,
                           config.arrow_width, height());
    AdjustBoundsForRTLUI(&arrow_bounds);
    canvas->DrawImageInt(*GetSubmenuArrowImage(),
                         arrow_bounds.x(), arrow_bounds.y());
  }
}

}  // namespace views
