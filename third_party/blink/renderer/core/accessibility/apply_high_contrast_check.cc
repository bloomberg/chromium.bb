// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/apply_high_contrast_check.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/high_contrast_color_classifier.h"

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

bool ShouldApplyHighContrastFilterToPage(
    HighContrastPagePolicy policy,
    const LayoutObject& root_layout_object) {
  switch (policy) {
    case HighContrastPagePolicy::kFilterAll:
      return true;
    case HighContrastPagePolicy::kFilterByBackground:
      return HasLightBackground(root_layout_object);
  }
}

}  // namespace blink
