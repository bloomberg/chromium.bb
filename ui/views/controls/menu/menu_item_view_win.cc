// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_item_view.h"

#include <uxtheme.h>
#include <Vssym32.h>

#include "grit/ui_strings.h"
#include "ui/base/native_theme/native_theme_win.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/submenu_view.h"

using ui::NativeTheme;

namespace views {

void MenuItemView::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  const MenuConfig& config = MenuConfig::instance();
  bool render_selection =
      (mode == PB_NORMAL && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this) &&
       !has_children());
  int default_sys_color;
  int state;
  NativeTheme::State control_state;

  if (enabled()) {
    if (render_selection) {
      control_state = NativeTheme::kHovered;
      state = MPI_HOT;
      default_sys_color = COLOR_HIGHLIGHTTEXT;
    } else {
      control_state = NativeTheme::kNormal;
      state = MPI_NORMAL;
      default_sys_color = COLOR_MENUTEXT;
    }
  } else {
    state = MPI_DISABLED;
    default_sys_color = COLOR_GRAYTEXT;
    control_state = NativeTheme::kDisabled;
  }

  // The gutter is rendered before the background.
  if (config.render_gutter && mode == PB_NORMAL) {
    gfx::Rect gutter_bounds(label_start_ - config.gutter_to_label -
                            config.gutter_width, 0, config.gutter_width,
                            height());
    AdjustBoundsForRTLUI(&gutter_bounds);
    NativeTheme::ExtraParams extra;
    NativeTheme::instance()->Paint(canvas->sk_canvas(),
                                   NativeTheme::kMenuPopupGutter,
                                   NativeTheme::kNormal,
                                   gutter_bounds,
                                   extra);
  }

  // Render the background.
  if (mode == PB_NORMAL) {
    gfx::Rect item_bounds(0, 0, width(), height());
    NativeTheme::ExtraParams extra;
    extra.menu_item.is_selected = render_selection;
    AdjustBoundsForRTLUI(&item_bounds);
    NativeTheme::instance()->Paint(canvas->sk_canvas(),
        NativeTheme::kMenuItemBackground, control_state, item_bounds, extra);
  }

  int top_margin = GetTopMargin();
  int bottom_margin = GetBottomMargin();

  if ((type_ == RADIO || type_ == CHECKBOX) &&
      GetDelegate()->IsItemChecked(GetCommand())) {
    PaintCheck(canvas, control_state, render_selection ? SELECTED : UNSELECTED,
               config);
  }

  // Render the foreground.
  // Menu color is specific to Vista, fallback to classic colors if can't
  // get color.
  SkColor fg_color = ui::NativeThemeWin::instance()->GetThemeColorWithDefault(
      ui::NativeThemeWin::MENU, MENU_POPUPITEM, state, TMT_TEXTCOLOR,
      default_sys_color);
  const gfx::Font& font = GetFont();
  int accel_width = parent_menu_item_->GetSubmenu()->max_accelerator_width();
  int width = this->width() - item_right_margin_ - label_start_ - accel_width;
  gfx::Rect text_bounds(label_start_, top_margin, width, font.GetHeight());
  text_bounds.set_x(GetMirroredXForRect(text_bounds));
  if (mode == PB_FOR_DRAG) {
    // With different themes, it's difficult to tell what the correct
    // foreground and background colors are for the text to draw the correct
    // halo. Instead, just draw black on white, which will look good in most
    // cases.
    canvas->DrawStringWithHalo(title(), font, 0x00000000, 0xFFFFFFFF,
        text_bounds.x(), text_bounds.y(), text_bounds.width(),
        text_bounds.height(), GetRootMenuItem()->GetDrawStringFlags());
  } else {
    canvas->DrawStringInt(title(), font, fg_color,
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
    int state_id = enabled() ? MSM_NORMAL : MSM_DISABLED;
    gfx::Rect arrow_bounds(this->width() - item_right_margin_ +
                           config.label_to_arrow_padding, 0,
                           config.arrow_width, height());
    AdjustBoundsForRTLUI(&arrow_bounds);

    // If our sub menus open from right to left (which is the case when the
    // locale is RTL) then we should make sure the menu arrow points to the
    // right direction.
    ui::NativeTheme::ExtraParams extra;
    extra.menu_arrow.pointing_right = !base::i18n::IsRTL();
    extra.menu_arrow.is_selected = render_selection;
    ui::NativeTheme::instance()->Paint(canvas->sk_canvas(),
        ui::NativeTheme::kMenuPopupArrow, control_state, arrow_bounds, extra);
  }
}

void MenuItemView::PaintCheck(gfx::Canvas* canvas,
                              NativeTheme::State state,
                              SelectionState selection_state,
                              const MenuConfig& config) {
  int icon_width;
  int icon_height;
  if (type_ == RADIO) {
    icon_width = config.radio_width;
    icon_height = config.radio_height;
  } else {
    icon_width = config.check_width;
    icon_height = config.check_height;
  }

  int top_margin = GetTopMargin();
  int icon_x = MenuConfig::instance().item_left_margin;
  int icon_y = top_margin +
      (height() - top_margin - GetBottomMargin() - icon_height) / 2;
  NativeTheme::ExtraParams extra;
  extra.menu_check.is_radio = type_ == RADIO;
  extra.menu_check.is_selected = selection_state == SELECTED;

  // Draw the background.
  gfx::Rect bg_bounds(0, 0, icon_x + icon_width, height());
  AdjustBoundsForRTLUI(&bg_bounds);
  NativeTheme::instance()->Paint(canvas->sk_canvas(),
      NativeTheme::kMenuCheckBackground, state, bg_bounds, extra);

  // And the check.
  gfx::Rect icon_bounds(icon_x / 2, icon_y, icon_width, icon_height);
  AdjustBoundsForRTLUI(&icon_bounds);
  NativeTheme::instance()->Paint(canvas->sk_canvas(),
      NativeTheme::kMenuCheck, state, bg_bounds, extra);
}

}  // namespace views
