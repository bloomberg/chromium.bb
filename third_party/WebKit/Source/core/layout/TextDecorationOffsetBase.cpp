// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style_ license that can be
// found in the LICENSE file.

#include "core/layout/TextDecorationOffsetBase.h"

#include <algorithm>
#include "core/layout/line/LineVerticalPositionType.h"
#include "core/paint/DecorationInfo.h"
#include "platform/fonts/FontMetrics.h"

namespace blink {

int TextDecorationOffsetBase::ComputeUnderlineOffsetForRoman(
    const FontMetrics& font_metrics,
    float text_decoration_thickness) const {
  // Compute the gap between the font and the underline. Use at least one
  // pixel gap, if underline is thick then use a bigger gap.
  int gap = 0;

  // Underline position of zero means draw underline on Baseline Position,
  // in Blink we need at least 1-pixel gap to adding following check.
  // Positive underline Position means underline should be drawn above baseline
  // and negative value means drawing below baseline, negating the value as in
  // Blink downward Y-increases.

  if (font_metrics.UnderlinePosition())
    gap = -font_metrics.UnderlinePosition();
  else
    gap = std::max<int>(1, ceilf(text_decoration_thickness / 2.f));

  // Position underline near the alphabetic baseline.
  return font_metrics.Ascent() + gap;
}

int TextDecorationOffsetBase::ComputeUnderlineOffset(
    ResolvedUnderlinePosition underline_position,
    const FontMetrics& font_metrics,
    float text_decoration_thickness) const {
  switch (underline_position) {
    default:
      NOTREACHED();
    // Fall through.
    case ResolvedUnderlinePosition::kRoman:
      return ComputeUnderlineOffsetForRoman(font_metrics,
                                            text_decoration_thickness);
    case ResolvedUnderlinePosition::kUnder:
      // Position underline at the under edge of the lowest element's
      // content box.
      return ComputeUnderlineOffsetForUnder(
          text_decoration_thickness,
          LineVerticalPositionType::BottomOfEmHeight);
  }
}

}  // namespace blink
