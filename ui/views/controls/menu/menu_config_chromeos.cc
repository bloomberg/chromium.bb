// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/controls/menu/menu_image_util.h"

namespace views {

void MenuConfig::Init() {
  if (ui::MaterialDesignController::IsRefreshUi()) {
    InitMaterialMenuConfig();
  } else {
    align_arrow_and_shortcut = true;
    gfx::ImageSkia check = GetMenuCheckImage(false);
    check_height = check.height();
    corner_radius = 2;
    separator_spacing_height = 7;
    separator_lower_height = 8;
    separator_upper_height = 8;
    submenu_horizontal_inset = 1;
    // In Ash, the border is provided by the shadow.
    use_outer_border = false;
  }
  always_use_icon_to_label_padding = true;
  arrow_to_edge_padding = 21;
  item_min_height = 29;
  offset_context_menus = true;
}

}  // namespace views
