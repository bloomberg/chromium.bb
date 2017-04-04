// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_line_height_metrics.h"

#include "core/style/ComputedStyle.h"

namespace blink {

NGLineHeightMetrics::NGLineHeightMetrics(const ComputedStyle& style,
                                         FontBaseline baseline_type) {
  const SimpleFontData* font_data = style.font().primaryFont();
  DCHECK(font_data);
  Initialize(font_data->getFontMetrics(), baseline_type,
             style.computedLineHeightAsFixed());
}

NGLineHeightMetrics::NGLineHeightMetrics(const FontMetrics& font_metrics,
                                         FontBaseline baseline_type) {
  Initialize(font_metrics, baseline_type,
             LayoutUnit::fromFloatCeil(font_metrics.floatLineSpacing()));
}

void NGLineHeightMetrics::Initialize(const FontMetrics& font_metrics,
                                     FontBaseline baseline_type,
                                     LayoutUnit line_height) {
  ascent = font_metrics.fixedAscent(baseline_type);
  descent = font_metrics.fixedDescent(baseline_type);
  LayoutUnit half_leading = (line_height - (ascent + descent)) / 2;
  // Ensure the top and the baseline is snapped to CSS pixel.
  // TODO(kojii): Snap baseline to pixel when our own paitn code is ready.
  ascent_and_leading = ascent + half_leading.floor();
  descent_and_leading = line_height - ascent_and_leading;
}

void NGLineHeightMetrics::Unite(const NGLineHeightMetrics& other) {
  ascent = std::max(ascent, other.ascent);
  descent = std::max(descent, other.descent);
  ascent_and_leading = std::max(ascent_and_leading, other.ascent_and_leading);
  descent_and_leading =
      std::max(descent_and_leading, other.descent_and_leading);
}

}  // namespace blink
