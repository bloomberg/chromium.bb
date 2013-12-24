// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/dpi_setup.h"

#include "base/command_line.h"
#include "ui/base/layout.h"
#include "ui/gfx/display.h"
#include "ui/gfx/win/dpi.h"

namespace ui {
namespace win {

void InitDeviceScaleFactor() {
  float scale = 1.0;
  if (CommandLine::ForCurrentProcess()->HasSwitch("silent-launch")) {
    if (gfx::IsHighDPIEnabled())
      scale = gfx::GetModernUIScale();
  }
  else {
    scale = gfx::GetDPIScale();
  }
  gfx::InitDeviceScaleFactor(scale);
}

}  // namespace win
}  // namespace ui
