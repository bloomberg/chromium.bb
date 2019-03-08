// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/apply_dark_mode.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {
namespace {

// TODO(https://crbug.com/925949): Add detection and classification of
// background image color. Most sites with dark background images also have a
// dark background color set, so this is less of a priority than it would be
// otherwise.
bool HasLightBackground(const LayoutObject& layout_object) {
  const ComputedStyle& style = layout_object.StyleRef();
  if (style.HasBackground()) {
    Color color = style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
    return IsLight(color, style.Opacity());
  }

  // If we can't easily determine the background color, default to inverting the
  // page.
  return true;
}

}  // namespace

DarkModeSettings BuildDarkModeSettings(const Settings& frame_settings,
                                       const LayoutObject& layout_object) {
  DarkModeSettings dark_mode_settings;

  if (!ShouldApplyDarkModeFilterToPage(frame_settings.GetDarkModePagePolicy(),
                                       layout_object)) {
    // In theory it should be sufficient to set mode to
    // kOff (or to just return the default struct) without also setting
    // image_policy. However, this causes images to be inverted unexpectedly in
    // some cases (such as when toggling between the site's light and dark theme
    // on arstechnica.com).
    //
    // TODO(gilmanmh): Investigate unexpected image inversion behavior when
    // image_policy not set to kFilterNone.
    dark_mode_settings.mode = DarkMode::kOff;
    dark_mode_settings.image_policy = DarkModeImagePolicy::kFilterNone;
    return dark_mode_settings;
  }
  dark_mode_settings.mode = frame_settings.GetDarkMode();
  dark_mode_settings.grayscale = frame_settings.GetDarkModeGrayscale();
  dark_mode_settings.contrast = frame_settings.GetDarkModeContrast();
  dark_mode_settings.image_policy = frame_settings.GetDarkModeImagePolicy();
  return dark_mode_settings;
}

bool ShouldApplyDarkModeFilterToPage(DarkModePagePolicy policy,
                                     const LayoutObject& root_layout_object) {
  switch (policy) {
    case DarkModePagePolicy::kFilterAll:
      return true;
    case DarkModePagePolicy::kFilterByBackground:
      return HasLightBackground(root_layout_object);
  }
}

}  // namespace blink
