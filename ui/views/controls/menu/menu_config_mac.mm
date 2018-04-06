// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "ui/base/material_design/material_design_controller.h"

namespace {

void InitMaterialMenuConfig(views::MenuConfig* config) {
  // These config parameters are from https://crbug.com/829347 and the spec
  // images linked from that bug.
  config->menu_vertical_border_size = 8;
  config->menu_horizontal_border_size = 0;
  config->submenu_horizontal_inset = 0;
  config->fixed_text_item_height = 32;
  config->fixed_container_item_height = 48;
  config->fixed_menu_width = 320;
  config->item_left_margin = 8;
  config->label_to_arrow_padding = 0;
  config->arrow_to_edge_padding = 16;
  config->icon_to_label_padding = 8;
  config->check_width = 16;
  config->check_height = 16;
  config->arrow_width = 8;
  config->separator_height = 17;
  config->separator_thickness = 1;
  config->label_to_minor_text_padding = 8;
  config->align_arrow_and_shortcut = true;
  config->use_outer_border = false;
  config->icons_in_label = true;
  config->corner_radius = 8;
}

}  // namespace

namespace views {

void MenuConfig::Init() {
  font_list = gfx::FontList(gfx::Font([NSFont menuFontOfSize:0.0]));
  check_selected_combobox_item = true;
  if (ui::MaterialDesignController::IsSecondaryUiMaterial())
    InitMaterialMenuConfig(this);
}

}  // namespace views
