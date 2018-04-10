// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "base/macros.h"
#include "ui/views/controls/menu/menu_image_util.h"
#include "ui/views/round_rect_painter.h"
namespace views {

MenuConfig::MenuConfig()
    : arrow_color(SK_ColorBLACK),
      menu_vertical_border_size(3),
      menu_horizontal_border_size(views::RoundRectPainter::kBorderWidth),
      submenu_horizontal_inset(3),
      item_top_margin(4),
      item_bottom_margin(3),
      item_no_icon_top_margin(4),
      item_no_icon_bottom_margin(4),
      fixed_text_item_height(0),
      fixed_menu_width(0),
      item_left_margin(10),
      touchable_item_left_margin(16),
      label_to_arrow_padding(10),
      arrow_to_edge_padding(5),
      icon_to_label_padding(10),
      touchable_icon_to_label_padding(22),
      touchable_icon_size(20),
      touchable_icon_color(SkColorSetA(SK_ColorBLACK, 0xDE)),
      check_width(kMenuCheckSize),
      check_height(kMenuCheckSize),
      arrow_width(kSubmenuArrowSize),
      separator_height(11),
      separator_upper_height(3),
      separator_lower_height(4),
      separator_spacing_height(3),
      separator_thickness(1),
      show_mnemonics(false),
      scroll_arrow_height(3),
      label_to_minor_text_padding(10),
      item_min_height(0),
      show_accelerators(true),
      always_use_icon_to_label_padding(false),
      align_arrow_and_shortcut(false),
      offset_context_menus(false),
      use_outer_border(true),
      icons_in_label(false),
      check_selected_combobox_item(false),
      show_delay(400),
      corner_radius(0),
      touchable_corner_radius(8),
      touchable_anchor_offset(8),
      touchable_menu_height(36),
      touchable_menu_width(256),
      touchable_menu_shadow_elevation(12),
      vertical_touchable_menu_item_padding(8) {
  Init();
}

MenuConfig::~MenuConfig() {}

// static
const MenuConfig& MenuConfig::instance() {
  CR_DEFINE_STATIC_LOCAL(MenuConfig, instance, ());
  return instance;
}

}  // namespace views
