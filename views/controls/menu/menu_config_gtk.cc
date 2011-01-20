// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_config.h"

#include "grit/app_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

namespace views {

// static
MenuConfig* MenuConfig::Create() {
  MenuConfig* config = new MenuConfig();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  config->font = rb.GetFont(ResourceBundle::BaseFont);
  config->arrow_width = rb.GetBitmapNamed(IDR_MENU_ARROW)->width();
  // Add 4 to force some padding between check and label.
  config->check_width = rb.GetBitmapNamed(IDR_MENU_CHECK)->width() + 4;
  config->check_height = rb.GetBitmapNamed(IDR_MENU_CHECK)->height();
  return config;
}

}  // namespace views
