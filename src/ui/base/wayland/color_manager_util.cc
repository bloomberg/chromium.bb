// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/wayland/color_manager_util.h"

namespace ui::wayland {

zcr_color_manager_v1_chromaticity_names ToColorManagerChromaticity(
    gfx::ColorSpace::PrimaryID primaryID) {
  for (const auto& it : kChromaticityMap) {
    if (it.second == primaryID)
      return it.first;
  }
  return ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_UNKNOWN;
}

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(
    gfx::ColorSpace::TransferID transferID) {
  for (const auto& it : kEotfMap) {
    if (it.second == transferID)
      return it.first;
  }
  return ZCR_COLOR_MANAGER_V1_EOTF_NAMES_UNKNOWN;
}

}  // namespace ui::wayland
