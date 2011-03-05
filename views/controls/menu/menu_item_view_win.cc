// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_item_view.h"

#include <uxtheme.h>
#include <Vssym32.h>

#include "grit/app_strings.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/native_theme_win.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/submenu_view.h"

using gfx::NativeTheme;

namespace views {

gfx::Size MenuItemView::CalculatePreferredSize() {
  const gfx::Font& font = MenuConfig::instance().font;
  return gfx::Size(
      font.GetStringWidth(title_) + label_start_ + item_right_margin_ +
          GetChildPreferredWidth(),
      font.GetHeight() + GetBottomMargin() + GetTopMargin());
}

void MenuItemView::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  const MenuConfig& config = MenuConfig::instance();
  bool render_selection =
      (mode == PB_NORMAL && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this) &&
       !has_children());
  int state = render_selection ? MPI_HOT :
                                 (IsEnabled() ? MPI_NORMAL : MPI_DISABLED);
  HDC dc = canvas->BeginPlatformPaint();
  NativeTheme::ControlState control_state;

  if (!IsEnabled()) {
    control_state = NativeTheme::CONTROL_DISABLED;
  } else {
    control_state = render_selection ? NativeTheme::CONTROL_HIGHLIGHTED :
                                       NativeTheme::CONTROL_NORMAL;
  }

  // The gutter is rendered before the background.
  if (config.render_gutter && mode == PB_NORMAL) {
    gfx::Rect gutter_bounds(label_start_ - config.gutter_to_label -
                            config.gutter_width, 0, config.gutter_width,
                            height());
    AdjustBoundsForRTLUI(&gutter_bounds);
    RECT gutter_rect = gutter_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuGutter(dc, MENU_POPUPGUTTER, MPI_NORMAL,
                                             &gutter_rect);
  }

  // Render the background.
  if (mode == PB_NORMAL) {
    gfx::Rect item_bounds(0, 0, width(), height());
    AdjustBoundsForRTLUI(&item_bounds);
    RECT item_rect = item_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuItemBackground(
        NativeTheme::MENU, dc, MENU_POPUPITEM, state, render_selection,
        &item_rect);
  }

  int top_margin = GetTopMargin();
  int bottom_margin = GetBottomMargin();

  if (type_ == CHECKBOX && GetDelegate()->IsItemChecked(GetCommand())) {
    PaintCheck(dc,
               IsEnabled() ? MC_CHECKMARKNORMAL : MC_CHECKMARKDISABLED,
               control_state, config.check_height, config.check_width);
  } else if (type_ == RADIO && GetDelegate()->IsItemChecked(GetCommand())) {
    PaintCheck(dc, IsEnabled() ? MC_BULLETNORMAL : MC_BULLETDISABLED,
               control_state, config.radio_height, config.radio_width);
  }

  // Render the foreground.
  // Menu color is specific to Vista, fallback to classic colors if can't
  // get color.
  int default_sys_color = render_selection ? COLOR_HIGHLIGHTTEXT :
      (IsEnabled() ? COLOR_MENUTEXT : COLOR_GRAYTEXT);
  SkColor fg_color = NativeTheme::instance()->GetThemeColorWithDefault(
      NativeTheme::MENU, MENU_POPUPITEM, state, TMT_TEXTCOLOR,
      default_sys_color);
  const gfx::Font& font = MenuConfig::instance().font;
  int accel_width = parent_menu_item_->GetSubmenu()->max_accelerator_width();
  int width = this->width() - item_right_margin_ - label_start_ - accel_width;
  gfx::Rect text_bounds(label_start_, top_margin, width, font.GetHeight());
  text_bounds.set_x(GetMirroredXForRect(text_bounds));
  if (mode == PB_FOR_DRAG) {
    // With different themes, it's difficult to tell what the correct
    // foreground and background colors are for the text to draw the correct
    // halo. Instead, just draw black on white, which will look good in most
    // cases.
    canvas->AsCanvasSkia()->DrawStringWithHalo(
        GetTitle(), font, 0x00000000, 0xFFFFFFFF, text_bounds.x(),
        text_bounds.y(), text_bounds.width(), text_bounds.height(),
        GetRootMenuItem()->GetDrawStringFlags());
  } else {
    canvas->DrawStringInt(GetTitle(), font, fg_color,
                          text_bounds.x(), text_bounds.y(), text_bounds.width(),
                          text_bounds.height(),
                          GetRootMenuItem()->GetDrawStringFlags());
  }

  PaintAccelerator(canvas);

  if (icon_.width() > 0) {
    gfx::Rect icon_bounds(config.item_left_margin,
                          top_margin + (height() - top_margin -
                          bottom_margin - icon_.height()) / 2,
                          icon_.width(),
                          icon_.height());
    icon_bounds.set_x(GetMirroredXForRect(icon_bounds));
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
    if (base::i18n::IsRTL())
      arrow_direction = NativeTheme::LEFT_POINTING_ARROW;
    else
      arrow_direction = NativeTheme::RIGHT_POINTING_ARROW;

    RECT arrow_rect = arrow_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuArrow(
        NativeTheme::MENU, dc, MENU_POPUPSUBMENU, state_id, &arrow_rect,
        arrow_direction, control_state);
  }
  canvas->EndPlatformPaint();
}

void MenuItemView::PaintCheck(HDC dc,
                              int state_id,
                              NativeTheme::ControlState control_state,
                              int icon_width,
                              int icon_height) {
  int top_margin = GetTopMargin();
  int icon_x = MenuConfig::instance().item_left_margin;
  int icon_y = top_margin +
      (height() - top_margin - GetBottomMargin() - icon_height) / 2;
  // Draw the background.
  gfx::Rect bg_bounds(0, 0, icon_x + icon_width, height());
  int bg_state = IsEnabled() ? MCB_NORMAL : MCB_DISABLED;
  AdjustBoundsForRTLUI(&bg_bounds);
  RECT bg_rect = bg_bounds.ToRECT();
  NativeTheme::instance()->PaintMenuCheckBackground(
      NativeTheme::MENU, dc, MENU_POPUPCHECKBACKGROUND, bg_state,
      &bg_rect);

  // And the check.
  gfx::Rect icon_bounds(icon_x / 2, icon_y, icon_width, icon_height);
  AdjustBoundsForRTLUI(&icon_bounds);
  RECT icon_rect = icon_bounds.ToRECT();
  NativeTheme::instance()->PaintMenuCheck(
      NativeTheme::MENU, dc, MENU_POPUPCHECK, state_id, &icon_rect,
      control_state);
}

}  // namespace views
