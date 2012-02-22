// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_mode_support.h"

#include "ui/base/ui_base_switches.h"
#include "base/command_line.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

// static
bool TouchModeSupport::IsTouchOptimized() {
#if !defined(OS_WIN)
  // TODO(port): there is code in src/ui/base/touch for linux but it is not
  // suitable for consumption here.
  return false;
#else
  // TODO: Enable based on GetSystemMetrics(SM_DIGITIZER) returning NID_READY
  // and NID_INTEGRATED_TOUCH once code is ready.
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTouchOptimizedUI);
#endif
}
