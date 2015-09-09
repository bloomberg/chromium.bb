// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#import <AppKit/AppKit.h>

#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme_mac.h"
#include "ui/views/controls/menu/menu_image_util.h"

namespace views {

void MenuConfig::Init(const ui::NativeTheme* theme) {
  font_list = gfx::FontList(gfx::Font([NSFont menuFontOfSize:0.0]));
  menu_vertical_border_size = 4;
  item_top_margin = item_no_icon_top_margin = 1;
  item_bottom_margin = item_no_icon_bottom_margin = 1;
  item_left_margin = 2;
  arrow_to_edge_padding = 12;
  icon_to_label_padding = 4;
  check_width = 19;
  check_height = 11;
  separator_height = 13;
  separator_upper_height = 7;
  separator_lower_height = 6;
  align_arrow_and_shortcut = true;
  icons_in_label = true;
  check_selected_combobox_item = true;
  corner_radius = 5;
  use_outer_border = false;
}

// static
const MenuConfig& MenuConfig::instance(const ui::NativeTheme* theme) {
  CR_DEFINE_STATIC_LOCAL(MenuConfig, mac_instance,
                         (theme ? theme : ui::NativeTheme::instance()));
  return mac_instance;
}

}  // namespace views
