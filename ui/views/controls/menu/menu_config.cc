// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "build/build_config.h"

namespace views {

static MenuConfig* config_instance = NULL;

MenuConfig::MenuConfig()
    : text_color(SK_ColorBLACK),
      item_top_margin(3),
      item_bottom_margin(4),
      item_no_icon_top_margin(1),
      item_no_icon_bottom_margin(3),
      item_left_margin(4),
      label_to_arrow_padding(10),
      arrow_to_edge_padding(5),
      icon_to_label_padding(8),
      gutter_to_label(5),
      check_width(16),
      check_height(16),
      radio_width(16),
      radio_height(16),
      arrow_height(9),
      arrow_width(9),
      gutter_width(0),
      separator_height(6),
      render_gutter(false),
      show_mnemonics(false),
      scroll_arrow_height(3),
      label_to_accelerator_padding(10),
      show_accelerators(true) {
}

MenuConfig::~MenuConfig() {}

void MenuConfig::Reset() {
  delete config_instance;
  config_instance = NULL;
}

// static
const MenuConfig& MenuConfig::instance() {
  if (!config_instance)
    config_instance = Create();
  return *config_instance;
}

}  // namespace views
