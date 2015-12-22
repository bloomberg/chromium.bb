// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_image_util.h"

#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/resources/grit/views_resources.h"

namespace views {

gfx::ImageSkia GetMenuCheckImage(SkColor icon_color) {
  return gfx::CreateVectorIcon(gfx::VectorIconId::MENU_CHECK, kMenuCheckSize,
                               icon_color);
}

gfx::ImageSkia GetRadioButtonImage(bool selected) {
  int image_id = selected ? IDR_MENU_RADIO_SELECTED : IDR_MENU_RADIO_EMPTY;
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(image_id).
      AsImageSkia();
}

gfx::ImageSkia GetSubmenuArrowImage(SkColor icon_color) {
  return gfx::CreateVectorIcon(gfx::VectorIconId::SUBMENU_ARROW,
                               kSubmenuArrowSize, icon_color);
}

}  // namespace views
