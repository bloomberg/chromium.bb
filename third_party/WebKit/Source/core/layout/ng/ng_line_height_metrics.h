// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineHeightMetrics_h
#define NGLineHeightMetrics_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "platform/fonts/FontBaseline.h"

namespace blink {

class ComputedStyle;
class FontMetrics;

// Represents line-progression metrics for line boxes and inline boxes.
// Computed for inline boxes, then the metrics of inline boxes are united to
// compute metrics for line boxes.
// https://drafts.csswg.org/css2/visudet.html#line-height
struct NGLineHeightMetrics {
  NGLineHeightMetrics() {}

  // Use the leading from the 'line-height' property, or the font metrics of
  // the primary font if 'line-height: normal'.
  NGLineHeightMetrics(const ComputedStyle&, FontBaseline);

  // Use the leading from the font metrics.
  NGLineHeightMetrics(const FontMetrics&, FontBaseline);

  // Unite a metrics for an inline box to a metrics for a line box.
  void Unite(const NGLineHeightMetrics&);

  // Ascent and descent of glyphs, or synthesized for replaced elements.
  // Then united to compute 'text-top' and 'text-bottom' of line boxes.
  LayoutUnit ascent;
  LayoutUnit descent;

  // Half the leading added to ascent and descent; A' = A + L/2 and
  // D' = D + L/2. https://drafts.csswg.org/css2/visudet.html#leading
  // Then united to compute 'top' and 'bottom' of line boxes. When united,
  // the half leading for ascent and for descent may be different.
  // TODO(kojii): Always carrying/uniting 4 values look inefficient.
  LayoutUnit ascent_and_leading;
  LayoutUnit descent_and_leading;

  LayoutUnit LineHeight() const {
    return ascent_and_leading + descent_and_leading;
  }

 private:
  void Initialize(const FontMetrics&, FontBaseline, LayoutUnit line_height);
};

}  // namespace blink

#endif  // NGLineHeightMetrics_h
