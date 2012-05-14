// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/ui_base_switches.h"

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/base/touch/touch_factory.h"
#endif // defined(USE_AURA) && defined(USE_X11)

#if defined(OS_WIN)
#include "base/win/metro.h"
#include <Windows.h>
#endif  // defined(OS_WIN)

namespace {
// Helper function that determines whether we want to optimize the UI for touch.
bool UseTouchOptimizedUI() {
   // If --touch-optimized-ui is specified and not set to "auto", then override
   // the hardware-determined setting (eg. for testing purposes).
   if (CommandLine::ForCurrentProcess()->HasSwitch(
       switches::kTouchOptimizedUI)) {
     const std::string switch_value = CommandLine::ForCurrentProcess()->
         GetSwitchValueASCII(switches::kTouchOptimizedUI);

     // Note that simply specifying the switch is the same as enabled.
     if (switch_value.empty() ||
         switch_value == switches::kTouchOptimizedUIEnabled) {
       return true;
     } else if (switch_value == switches::kTouchOptimizedUIDisabled) {
       return false;
     } else if (switch_value != switches::kTouchOptimizedUIAuto) {
       LOG(ERROR) << "Invalid --touch-optimized-ui option: " << switch_value;
     }
   }

#if defined(ENABLE_METRO)
  return base::win::GetMetroModule() != NULL;
#elif defined(OS_WIN)
  // No touch support on Win outside Metro yet (even with Aura).
  return false;
#elif defined(USE_AURA) && defined(USE_X11)
  // Determine whether touch-screen hardware is currently available.
  // For now we assume this won't change over the life of the process, but
  // we'll probably want to support that.  crbug.com/124399
  return ui::TouchFactory::GetInstance()->IsTouchDevicePresent();
#else
  return false;
#endif
}
}

namespace ui {

// Note that this function should be extended to select
// LAYOUT_TOUCH when appropriate on more platforms than just
// Windows and Ash.
DisplayLayout GetDisplayLayout() {
#if defined(USE_ASH)
  if (UseTouchOptimizedUI())
    return LAYOUT_TOUCH;
  return LAYOUT_ASH;
#elif defined(OS_WIN)
  if (UseTouchOptimizedUI())
    return LAYOUT_TOUCH;
  return LAYOUT_DESKTOP;
#else
  return LAYOUT_DESKTOP;
#endif
}

}  // namespace ui
