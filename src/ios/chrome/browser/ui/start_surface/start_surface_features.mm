// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#include "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kStartSurface{"StartSurface",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kStartSurfaceSplashStartup{
    "StartSurfaceSplashStartup", base::FEATURE_ENABLED_BY_DEFAULT};

const char kReturnToStartSurfaceInactiveDurationInSeconds[] =
    "ReturnToStartSurfaceInactiveDurationInSeconds";

const char kStartSurfaceHideShortcutsParam[] = "remove_shortcuts";
const char kStartSurfaceShrinkLogoParam[] = "shrink_logo";
const char kStartSurfaceReturnToRecentTabParam[] = "show_return_to_recent_tab";

bool IsStartSurfaceEnabled() {
  return base::FeatureList::IsEnabled(kStartSurface);
}

bool IsStartSurfaceSplashStartupEnabled() {
  return base::FeatureList::IsEnabled(kStartSurfaceSplashStartup);
}

double GetReturnToStartSurfaceDuration() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kStartSurface, kReturnToStartSurfaceInactiveDurationInSeconds,
      60 * 60 * 12 /*default to 12 hour*/);
}

bool ShouldHideShortcutsForStartSurface() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kStartSurface, kStartSurfaceHideShortcutsParam, false);
}

bool ShouldShrinkLogoForStartSurface() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kStartSurface, kStartSurfaceShrinkLogoParam, true);
}

bool ShouldShowReturnToMostRecentTabForStartSurface() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kStartSurface, kStartSurfaceReturnToRecentTabParam, true);
}
