// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "ui/base/material_design/material_design_controller.h"

namespace views {

void MenuConfig::Init() {
  if (ui::MaterialDesignController::IsRefreshUi())
    InitMaterialMenuConfig();
  else
    arrow_to_edge_padding = 6;
}

}  // namespace views
