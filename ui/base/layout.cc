// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include "base/basictypes.h"
#include "build/build_config.h"

#if defined(USE_ASH)
#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#endif

#if defined(OS_WIN)
#include "base/win/metro.h"
#include <Windows.h>
#endif  // defined(OS_WIN)

namespace ui {

// Note that this function should be extended to select
// LAYOUT_TOUCH when appropriate on more platforms than just
// Windows.
DisplayLayout GetDisplayLayout() {
#if defined(USE_ASH)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTouchOptimizedUI))
    return LAYOUT_TOUCH;
  return LAYOUT_ASH;
#elif !defined(OS_WIN)
  return LAYOUT_DESKTOP;
#else  // On Windows.
  bool use_touch = false;
#if defined(ENABLE_METRO)
  use_touch = base::win::GetMetroModule() != NULL;
#endif  // defined(ENABLE_METRO)
  if (use_touch) {
    return LAYOUT_TOUCH;
  } else {
    return LAYOUT_DESKTOP;
  }
#endif  // On Windows.
}

}  // namespace ui
