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
  // TODO: This may need to be cached and updated only occasionally. There is
  // no direct WM_ message that indicates that a touch panel has been added or
  // removed, however.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTouchOptimizedUI))
    return true;
  // Asking for SM_DIGITIZER always returns 0 in Windows Vista and Windows XP.
  int sm = ::GetSystemMetrics(SM_DIGITIZER);
  return (sm & NID_READY) && (sm & NID_INTEGRATED_TOUCH);
#endif
}
