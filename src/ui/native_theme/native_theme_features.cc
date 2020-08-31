// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_features.h"

#include "build/build_config.h"

namespace features {

#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// Enables or disables overlay scrollbars in Blink (i.e. web content) on Aura
// or Linux.  The status of native UI overlay scrollbars is determined in
// PlatformStyle::CreateScrollBar. Does nothing on Mac.
const base::Feature kOverlayScrollbar{"OverlayScrollbar",
                                      kOverlayScrollbarFeatureState};

// Enables will flash all scrollbars in page after any scroll update.
const base::Feature kOverlayScrollbarFlashAfterAnyScrollUpdate{
    "OverlayScrollbarFlashAfterAnyScrollUpdate", kOverlayScrollbarFeatureState};

// Experiment: Enables will flash scorllbar when user move mouse enter a
// scrollable area.
const base::Feature kOverlayScrollbarFlashWhenMouseEnter{
    "OverlayScrollbarFlashWhenMouseEnter", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace ui {

bool IsOverlayScrollbarEnabled() {
  return base::FeatureList::IsEnabled(features::kOverlayScrollbar);
}

bool OverlayScrollbarFlashAfterAnyScrollUpdate() {
  return base::FeatureList::IsEnabled(
      features::kOverlayScrollbarFlashAfterAnyScrollUpdate);
}

bool OverlayScrollbarFlashWhenMouseEnter() {
  return base::FeatureList::IsEnabled(
      features::kOverlayScrollbarFlashWhenMouseEnter);
}

}  // namespace ui
