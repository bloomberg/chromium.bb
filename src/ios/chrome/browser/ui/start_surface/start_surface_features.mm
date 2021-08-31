// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#include "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kStartSurface{"StartSurface",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const char kReturnToStartSurfaceInactiveDurationInSeconds[] =
    "ReturnToStartSurfaceInactiveDurationInSeconds";

const char kStartSurfaceHideShortcutsParam[] = "remove_shortcuts";
const char kStartSurfaceShrinkLogoParam[] = "shrink_logo";
const char kStartSurfaceReturnToRecentTabParam[] = "show_return_to_recent_tab";

bool IsStartSurfaceEnabled() {
  return base::FeatureList::IsEnabled(kStartSurface);
}

double GetReturnToStartSurfaceDuration() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kStartSurface, kReturnToStartSurfaceInactiveDurationInSeconds,
      60 * 60 /*default to 1 hour*/);
}

bool ShouldHideShortcutsForStartSurface() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kStartSurface, kStartSurfaceHideShortcutsParam, false);
}

bool ShouldShrinkLogoForStartSurface() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kStartSurface, kStartSurfaceShrinkLogoParam, false);
}

bool ShouldShowReturnToMostRecentTabForStartSurface() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kStartSurface, kStartSurfaceReturnToRecentTabParam, false);
}
