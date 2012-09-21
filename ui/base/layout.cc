// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/ui_base_switches.h"

#if defined(USE_AURA) && !defined(OS_WIN)
#include "ui/aura/root_window.h"
#include "ui/compositor/compositor.h"
#endif  // defined(USE_AURA) && !defined(OS_WIN)

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

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
#else
  return false;
#endif
}

const float kScaleFactorScales[] = {1.0f, 1.4f, 1.8f, 2.0f};
COMPILE_ASSERT(ui::NUM_SCALE_FACTORS == arraysize(kScaleFactorScales),
               kScaleFactorScales_incorrect_size);
const size_t kScaleFactorScalesLength = arraysize(kScaleFactorScales);

std::vector<ui::ScaleFactor>& GetSupportedScaleFactorsInternal() {
  static std::vector<ui::ScaleFactor>* supported_scale_factors =
      new std::vector<ui::ScaleFactor>();
  if (supported_scale_factors->empty()) {
      supported_scale_factors->push_back(ui::SCALE_FACTOR_100P);
// TODO(rohitrao): Set the appropriate scale factors for iOS.  Ideally set
// either 100P or 200P but not both, since a given device will only ever use one
// scale factor.
#if defined(OS_MACOSX) && !defined(OS_IOS) && defined(ENABLE_HIDPI)
      if (base::mac::IsOSLionOrLater())
        supported_scale_factors->push_back(ui::SCALE_FACTOR_200P);
#elif defined(OS_WIN) && defined(ENABLE_HIDPI)
      if (base::win::IsMetroProcess() && base::win::IsTouchEnabled()) {
        supported_scale_factors->push_back(ui::SCALE_FACTOR_140P);
        supported_scale_factors->push_back(ui::SCALE_FACTOR_180P);
      }
#elif defined(USE_ASH)
      supported_scale_factors->push_back(ui::SCALE_FACTOR_200P);
#endif
  }
  return *supported_scale_factors;
}

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

std::vector<ScaleFactor> GetSupportedScaleFactors() {
  return GetSupportedScaleFactorsInternal();
}

bool IsScaleFactorSupported(ScaleFactor scale_factor) {
  const std::vector<ScaleFactor>& supported =
      GetSupportedScaleFactorsInternal();
  return std::find(supported.begin(), supported.end(), scale_factor) !=
      supported.end();
}

namespace test {

void SetSupportedScaleFactors(
    const std::vector<ui::ScaleFactor>& scale_factors) {
  std::vector<ui::ScaleFactor>& supported_scale_factors =
      GetSupportedScaleFactorsInternal();
  supported_scale_factors = scale_factors;
}

}  // namespace test

#if !defined(OS_MACOSX)
ScaleFactor GetScaleFactorForNativeView(gfx::NativeView view) {
#if defined(USE_AURA) && !defined(OS_WIN)
  aura::RootWindow* root_window = view->GetRootWindow();
  if (!root_window)
    return SCALE_FACTOR_NONE;
  return GetScaleFactorFromScale(
      root_window->compositor()->device_scale_factor());
#else
  NOTIMPLEMENTED();
  return SCALE_FACTOR_NONE;
#endif
}
#endif  // !defined(OS_MACOSX)

}  // namespace ui
