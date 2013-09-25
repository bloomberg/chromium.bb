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
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#include <Windows.h>
#endif  // defined(OS_WIN)

namespace ui {

namespace {

bool ScaleFactorComparator(const ScaleFactor& lhs, const ScaleFactor& rhs){
  return GetImageScale(lhs) < GetImageScale(rhs);
}

std::vector<ScaleFactor>* g_supported_scale_factors = NULL;

#if defined(OS_WIN)
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

  // We use the touch layout only when we are running in Metro mode.
  return base::win::IsMetroProcess() && ui::IsTouchDevicePresent();
}
#endif  // defined(OS_WIN)

const float kScaleFactorScales[] = {1.0f, 1.0f, 1.25f, 1.33f, 1.4f, 1.5f, 1.8f,
                                    2.0f};
COMPILE_ASSERT(NUM_SCALE_FACTORS == arraysize(kScaleFactorScales),
               kScaleFactorScales_incorrect_size);

}  // namespace

DisplayLayout GetDisplayLayout() {
#if defined(OS_WIN)
  if (UseTouchOptimizedUI())
    return LAYOUT_TOUCH;
#endif
  return LAYOUT_DESKTOP;
}

void SetSupportedScaleFactors(
    const std::vector<ui::ScaleFactor>& scale_factors) {
  if (g_supported_scale_factors != NULL)
    delete g_supported_scale_factors;

  g_supported_scale_factors = new std::vector<ScaleFactor>(scale_factors);
  std::sort(g_supported_scale_factors->begin(),
            g_supported_scale_factors->end(),
            ScaleFactorComparator);

  // Set ImageSkia's supported scales.
  std::vector<float> scales;
  for (std::vector<ScaleFactor>::const_iterator it =
          g_supported_scale_factors->begin();
       it != g_supported_scale_factors->end(); ++it) {
    scales.push_back(GetImageScale(*it));
  }
  gfx::ImageSkia::SetSupportedScales(scales);
}

const std::vector<ScaleFactor>& GetSupportedScaleFactors() {
  DCHECK(g_supported_scale_factors != NULL);
  return *g_supported_scale_factors;
}

ScaleFactor GetSupportedScaleFactor(float scale) {
  DCHECK(g_supported_scale_factors != NULL);
  ScaleFactor closest_match = SCALE_FACTOR_100P;
  float smallest_diff =  std::numeric_limits<float>::max();
  for (size_t i = 0; i < g_supported_scale_factors->size(); ++i) {
    ScaleFactor scale_factor = (*g_supported_scale_factors)[i];
    float diff = std::abs(kScaleFactorScales[scale_factor] - scale);
    if (diff < smallest_diff) {
      closest_match = scale_factor;
      smallest_diff = diff;
    }
  }
  DCHECK_NE(closest_match, SCALE_FACTOR_NONE);
  return closest_match;
}

float GetImageScale(ScaleFactor scale_factor) {
  return kScaleFactorScales[scale_factor];
}

bool IsScaleFactorSupported(ScaleFactor scale_factor) {
  DCHECK(g_supported_scale_factors != NULL);
  return std::find(g_supported_scale_factors->begin(),
                   g_supported_scale_factors->end(),
                   scale_factor) != g_supported_scale_factors->end();
}

// Returns the scale factor closest to |scale| from the full list of factors.
// Note that it does NOT rely on the list of supported scale factors.
// Finding the closest match is inefficient and shouldn't be done frequently.
ScaleFactor FindClosestScaleFactorUnsafe(float scale) {
  float smallest_diff =  std::numeric_limits<float>::max();
  ScaleFactor closest_match = SCALE_FACTOR_100P;
  for (int i = SCALE_FACTOR_100P; i < NUM_SCALE_FACTORS; ++i) {
    const ScaleFactor scale_factor = static_cast<ScaleFactor>(i);
    float diff = std::abs(kScaleFactorScales[scale_factor] - scale);
    if (diff < smallest_diff) {
      closest_match = scale_factor;
      smallest_diff = diff;
    }
  }
  return closest_match;
}

namespace test {

ScopedSetSupportedScaleFactors::ScopedSetSupportedScaleFactors(
    const std::vector<ui::ScaleFactor>& new_scale_factors) {
  if (g_supported_scale_factors) {
    original_scale_factors_ =
        new std::vector<ScaleFactor>(*g_supported_scale_factors);
  } else {
    original_scale_factors_ = NULL;
  }
  SetSupportedScaleFactors(new_scale_factors);
}

ScopedSetSupportedScaleFactors::~ScopedSetSupportedScaleFactors() {
  if (original_scale_factors_) {
    SetSupportedScaleFactors(*original_scale_factors_);
    delete original_scale_factors_;
  } else {
    delete g_supported_scale_factors;
    g_supported_scale_factors = NULL;
  }
}

}  // namespace test

#if !defined(OS_MACOSX)
ScaleFactor GetScaleFactorForNativeView(gfx::NativeView view) {
  gfx::Screen* screen = gfx::Screen::GetScreenFor(view);
  if (screen->IsDIPEnabled()) {
    gfx::Display display = screen->GetDisplayNearestWindow(view);
    return GetSupportedScaleFactor(display.device_scale_factor());
  }
  return ui::SCALE_FACTOR_100P;
}
#endif  // !defined(OS_MACOSX)

}  // namespace ui
