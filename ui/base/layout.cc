// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include <cmath>
#include <limits>

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
  static bool has_touch_optimized_ui = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kTouchOptimizedUI);
  if (has_touch_optimized_ui) {
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

#if defined(OS_WIN)
  // On Windows, we use the touch layout only when we are running in
  // Metro mode.
  return base::win::IsMetroProcess() && base::win::IsTouchEnabled();
#elif defined(USE_AURA) && defined(USE_X11)
  // Determine whether touch-screen hardware is currently available.
  // For now we must ensure this won't change over the life of the process,
  // since we don't yet support updating the UI.  crbug.com/124399
  static bool has_touch_device =
      ui::TouchFactory::GetInstance()->IsTouchDevicePresent();

  // Work-around for late device detection in some cases.  If we've asked for
  // touch calibration then we're certainly expecting a touch screen, it must
  // just not be ready yet.  Force-enable touch-ui mode in this case.
  static bool enable_touch_calibration = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableTouchCalibration);
  if (!has_touch_device && enable_touch_calibration)
    has_touch_device = true;

  return has_touch_device;
#else
  return false;
#endif
}

const float kScaleFactorScales[] = {1.0, 2.0};
const size_t kScaleFactorScalesLength = arraysize(kScaleFactorScales);

#if defined(OS_MACOSX)
std::vector<ui::ScaleFactor>& GetSupportedScaleFactorsInternal() {
  static std::vector<ui::ScaleFactor>* supported_scale_factors =
      new std::vector<ui::ScaleFactor>();
  if (supported_scale_factors->empty()) {
      supported_scale_factors->push_back(ui::SCALE_FACTOR_100P);
      supported_scale_factors->push_back(ui::SCALE_FACTOR_200P);
  }
  return *supported_scale_factors;
}
#endif  // OS_MACOSX

}  // namespace

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

ScaleFactor GetScaleFactorFromScale(float scale) {
  size_t closest_match = 0;
  float smallest_diff =  std::numeric_limits<float>::max();
  for (size_t i = 0; i < kScaleFactorScalesLength; ++i) {
    float diff = std::abs(kScaleFactorScales[i] - scale);
    if (diff < smallest_diff) {
      closest_match = i;
      smallest_diff = diff;
    }
  }
  return static_cast<ui::ScaleFactor>(closest_match);
}

float GetScaleFactorScale(ScaleFactor scale_factor) {
  return kScaleFactorScales[scale_factor];
}

#if defined(OS_MACOSX)
std::vector<ScaleFactor> GetSupportedScaleFactors() {
  return GetSupportedScaleFactorsInternal();
}

namespace test {

void SetSupportedScaleFactors(
    const std::vector<ui::ScaleFactor>& scale_factors) {
  std::vector<ui::ScaleFactor>& supported_scale_factors =
      GetSupportedScaleFactorsInternal();
  supported_scale_factors.clear();

  for (size_t i = 0; i < scale_factors.size(); ++i)
    supported_scale_factors.push_back(scale_factors[i]);
}

}  // namespace test

#endif  // OS_MACOSX

}  // namespace ui
