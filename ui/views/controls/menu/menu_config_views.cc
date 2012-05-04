// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_theme_aura.h"

namespace views {

// static
MenuConfig* MenuConfig::Create() {
  MenuConfig* config = new MenuConfig();
  config->text_color = gfx::NativeTheme::instance()->GetSystemColor(
      gfx::NativeTheme::kColorId_EnabledMenuItemForegroundColor);
  config->submenu_horizontal_margin_size = 0;
  config->submenu_vertical_margin_size = 2;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  config->font = rb.GetFont(ResourceBundle::BaseFont);
  config->arrow_width = rb.GetImageNamed(IDR_MENU_ARROW).ToSkBitmap()->width();
  const SkBitmap* check = rb.GetImageNamed(IDR_MENU_CHECK).ToSkBitmap();
  // Add 4 to force some padding between check and label.
  config->check_width = check->width() + 4;
  config->check_height = check->height();
  config->item_min_height = 30;

  return config;
}

}  // namespace views
