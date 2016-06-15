// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_MATERIAL_DESIGN_ICON_CONTROLLER_H_
#define UI_CHROMEOS_MATERIAL_DESIGN_ICON_CONTROLLER_H_

#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {
namespace md_icon_controller {

// Sets whether or not material design versions of the network icons should
// be used (WiFi, cellular network, VPN, and network badges). Called by
// ash::MaterialDesignController upon initialization.
UI_CHROMEOS_EXPORT void SetUseMaterialDesignNetworkIcons(bool use);

// Returns true if material design versions of the network icons should
// be used.
UI_CHROMEOS_EXPORT bool UseMaterialDesignNetworkIcons();

}  // namespace md_icon_controller
}  // namespace ui

#endif  // UI_CHROMEOS_MATERIAL_DESIGN_ICON_CONTROLLER_H_
