// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "ash/constants/ambient_animation_theme.h"
#include "base/metrics/histogram_functions.h"

namespace ash {
namespace personalization_app {

void LogPersonalizationTheme(ColorMode color_mode) {
  base::UmaHistogramEnumeration(kPersonalizationThemeColorModeHistogramName,
                                color_mode);
}

void LogAmbientModeAnimationTheme(ash::AmbientAnimationTheme animation_theme) {
  base::UmaHistogramEnumeration(kAmbientModeAnimationThemeHistogramName,
                                animation_theme);
}

}  // namespace personalization_app
}  // namespace ash
