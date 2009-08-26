// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_item_view.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "app/gfx/canvas.h"
#include "base/gfx/native_theme.h"
#include "app/l10n_util.h"
#include "grit/app_strings.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/submenu_view.h"

using gfx::NativeTheme;

namespace views {

gfx::Size MenuItemView::GetPreferredSize() {
  const gfx::Font& font = MenuConfig::instance().font;
  return gfx::Size(
      font.GetStringWidth(title_) + label_start_ + item_right_margin_,
      font.height() + GetBottomMargin() + GetTopMargin());
}

void MenuItemView::Paint(gfx::Canvas* canvas, bool for_drag) {
  const MenuConfig& config = MenuConfig::instance();
  bool render_selection =
      (!for_drag && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this));
  int state = render_selection ? MPI_HOT :
                                 (IsEnabled() ? MPI_NORMAL : MPI_DISABLED);
  HDC dc = canvas->beginPlatformPaint();

  // The gutter is rendered before the background.
  if (config.render_gutter && !for_drag) {
    gfx::Rect gutter_bounds(label_start_ - config.gutter_to_label -
                            config.gutter_width, 0, config.gutter_width,
                            height());
    AdjustBoundsForRTLUI(&gutter_bounds);
    RECT gutter_rect = gutter_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuGutter(dc, MENU_POPUPGUTTER, MPI_NORMAL,
                                             &gutter_rect);
  }

  // Render the background.
  if (!for_drag) {
    gfx::Rect item_bounds(0, 0, width(), height());
    AdjustBoundsForRTLUI(&item_bounds);
    RECT item_rect = item_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuItemBackground(
        NativeTheme::MENU, dc, MENU_POPUPITEM, state, render_selection,
        &item_rect);
  }

  int icon_x = config.item_left_margin;
  int top_margin = GetTopMargin();
  int bottom_margin = GetBottomMargin();
  int icon_y = top_margin + (height() - config.item_top_margin -
                             bottom_margin - config.check_height) / 2;
  int icon_height = config.check_height;
  int icon_width = config.check_width;

  if (type_ == CHECKBOX && GetDelegate()->IsItemChecked(GetCommand())) {
    // Draw the check background.
    gfx::Rect check_bg_bounds(0, 0, icon_x + icon_width, height());
    const int bg_state = IsEnabled() ? MCB_NORMAL : MCB_DISABLED;
    AdjustBoundsForRTLUI(&check_bg_bounds);
    RECT check_bg_rect = check_bg_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuCheckBackground(
        NativeTheme::MENU, dc, MENU_POPUPCHECKBACKGROUND, bg_state,
        &check_bg_rect);

    // And the check.
    gfx::Rect check_bounds(icon_x, icon_y, icon_width, icon_height);
    const int check_state = IsEnabled() ? MC_CHECKMARKNORMAL :
                                          MC_CHECKMARKDISABLED;
    AdjustBoundsForRTLUI(&check_bounds);
    RECT check_rect = check_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuCheck(
        NativeTheme::MENU, dc, MENU_POPUPCHECK, check_state, &check_rect,
        render_selection);
  }

  // Render the foreground.
  // Menu color is specific to Vista, fallback to classic colors if can't
  // get color.
  int default_sys_color = render_selection ? COLOR_HIGHLIGHTTEXT :
      (IsEnabled() ? COLOR_MENUTEXT : COLOR_GRAYTEXT);
  SkColor fg_color = NativeTheme::instance()->GetThemeColorWithDefault(
      NativeTheme::MENU, MENU_POPUPITEM, state, TMT_TEXTCOLOR,
      default_sys_color);
  int width = this->width() - item_right_margin_ - label_start_;
  const gfx::Font& font = MenuConfig::instance().font;
  gfx::Rect text_bounds(label_start_, top_margin, width, font.height());
  text_bounds.set_x(MirroredLeftPointForRect(text_bounds));
  if (for_drag) {
    // With different themes, it's difficult to tell what the correct
    // foreground and background colors are for the text to draw the correct
    // halo. Instead, just draw black on white, which will look good in most
    // cases.
    canvas->DrawStringWithHalo(GetTitle(), font, 0x00000000, 0xFFFFFFFF,
                               text_bounds.x(), text_bounds.y(),
                               text_bounds.width(), text_bounds.height(),
                               GetRootMenuItem()->GetDrawStringFlags());
  } else {
    canvas->DrawStringInt(GetTitle(), font, fg_color,
                          text_bounds.x(), text_bounds.y(), text_bounds.width(),
                          text_bounds.height(),
                          GetRootMenuItem()->GetDrawStringFlags());
  }

  if (icon_.width() > 0) {
    gfx::Rect icon_bounds(config.item_left_margin,
                          top_margin + (height() - top_margin -
                          bottom_margin - icon_.height()) / 2,
                          icon_.width(),
                          icon_.height());
    icon_bounds.set_x(MirroredLeftPointForRect(icon_bounds));
    canvas->DrawBitmapInt(icon_, icon_bounds.x(), icon_bounds.y());
  }

  if (HasSubmenu()) {
    int state_id = IsEnabled() ? MSM_NORMAL : MSM_DISABLED;
    gfx::Rect arrow_bounds(this->width() - item_right_margin_ +
                           config.label_to_arrow_padding, 0,
                           config.arrow_width, height());
    AdjustBoundsForRTLUI(&arrow_bounds);

    // If our sub menus open from right to left (which is the case when the
    // locale is RTL) then we should make sure the menu arrow points to the
    // right direction.
    NativeTheme::MenuArrowDirection arrow_direction;
    if (UILayoutIsRightToLeft())
      arrow_direction = NativeTheme::LEFT_POINTING_ARROW;
    else
      arrow_direction = NativeTheme::RIGHT_POINTING_ARROW;

    RECT arrow_rect = arrow_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuArrow(
        NativeTheme::MENU, dc, MENU_POPUPSUBMENU, state_id, &arrow_rect,
        arrow_direction, render_selection);
  }
  canvas->endPlatformPaint();
}

}  // namespace views
