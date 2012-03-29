// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_mode_support.h"

#include "ui/base/ui_base_switches.h"
#include "base/command_line.h"

// static
bool TouchModeSupport::IsTouchOptimized() {
  // TODO: Enable based on GetSystemMetrics(SM_DIGITIZER) returning NID_READY
  // and NID_INTEGRATED_TOUCH once code is ready (Only for WIN).
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTouchOptimizedUI);
}
