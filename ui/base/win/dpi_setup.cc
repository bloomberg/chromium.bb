// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/dpi_setup.h"

#include "ui/base/layout.h"
#include "ui/gfx/display.h"
#include "ui/gfx/win/dpi.h"

namespace ui {
namespace win {

void InitDeviceScaleFactor() {
  float scale = 1.0f;
  if (gfx::IsHighDPIEnabled()) {
    scale = gfx::Display::HasForceDeviceScaleFactor() ?
        gfx::Display::GetForcedDeviceScaleFactor() : gfx::GetDPIScale();
    // Quantize to nearest supported scale factor.
    scale = ui::GetImageScale(ui::GetSupportedScaleFactor(scale));
  }
  gfx::InitDeviceScaleFactor(scale);
}

}  // namespace win
}  // namespace ui
