// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/material_design_icon_controller.h"

namespace ui {
namespace md_icon_controller {

namespace {
bool use_md_network_icons = false;
}  // namespace

void SetUseMaterialDesignNetworkIcons(bool use) {
  use_md_network_icons = use;
}

bool UseMaterialDesignNetworkIcons() {
  return use_md_network_icons;
}

}  // namespace md_icon_controller
}  // namespace ui
