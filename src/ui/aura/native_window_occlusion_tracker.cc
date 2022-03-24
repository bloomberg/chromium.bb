// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/native_window_occlusion_tracker.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"

#if defined(OS_WIN)
#include "ui/aura/native_window_occlusion_tracker_win.h"
#endif  // OS_WIN

namespace aura {
namespace {
#if defined(OS_WIN)
// Whether IsNativeWindowOcclusionTrackingAlwaysEnabled() should check for
// CHROME_HEADLESS.
bool g_headless_check_enabled = true;
#endif
}  // namespace

// static
void NativeWindowOcclusionTracker::EnableNativeWindowOcclusionTracking(
    WindowTreeHost* host) {
#if defined(OS_WIN)
  if (host->IsNativeWindowOcclusionEnabled()) {
    NativeWindowOcclusionTrackerWin::GetOrCreateInstance()->Enable(
        host->window());
  }
#endif  // defined(OS_WIN)
}

// static
void NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(
    WindowTreeHost* host) {
#if defined(OS_WIN)
  if (host->IsNativeWindowOcclusionEnabled()) {
    host->SetNativeWindowOcclusionState(Window::OcclusionState::UNKNOWN, {});
    NativeWindowOcclusionTrackerWin::GetOrCreateInstance()->Disable(
        host->window());
  }
#endif  // defined(OS_WIN)
}

// static
bool NativeWindowOcclusionTracker::IsNativeWindowOcclusionTrackingAlwaysEnabled(
    WindowTreeHost* host) {
#if defined(OS_WIN)
  // chromedriver uses the environment variable CHROME_HEADLESS. In this case
  // it expected that native occlusion is not applied.
  static bool is_headless = getenv("CHROME_HEADLESS") != nullptr;
  if ((is_headless && g_headless_check_enabled) ||
      !host->IsNativeWindowOcclusionEnabled() ||
      !base::FeatureList::IsEnabled(features::kCalculateNativeWinOcclusion) ||
      !base::FeatureList::IsEnabled(
          features::kApplyNativeOcclusionToCompositor)) {
    return false;
  }
  const std::string type = base::GetFieldTrialParamValueByFeature(
      features::kApplyNativeOcclusionToCompositor,
      features::kApplyNativeOcclusionToCompositorType);
  return type == features::kApplyNativeOcclusionToCompositorTypeRelease ||
         type == features::kApplyNativeOcclusionToCompositorTypeThrottle;
#else
  return false;
#endif
}

#if defined(OS_WIN)
// static
void NativeWindowOcclusionTracker::SetHeadlessCheckEnabled(bool enabled) {
  g_headless_check_enabled = enabled;
}
#endif

}  // namespace aura
