// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"

namespace views {

void MenuConfig::Init() {
  InitMaterialMenuConfig();
  font_list = gfx::FontList(gfx::Font([NSFont menuFontOfSize:0.0]));
  check_selected_combobox_item = true;
  arrow_key_selection_wraps = false;
  use_mnemonics = false;
  show_context_menu_accelerators = false;
  all_menus_use_prefix_selection = true;
}

}  // namespace views
