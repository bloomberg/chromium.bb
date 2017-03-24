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
struct NGLineHeightMetrics {
  NGLineHeightMetrics() {}

  // Use the leading from the 'line-height' property, or the font metrics of
  // the primary font if 'line-height: normal'.
  NGLineHeightMetrics(const ComputedStyle&, FontBaseline);

  // Use the leading from the font metrics.
  NGLineHeightMetrics(const FontMetrics&, FontBaseline);

  void Unite(const NGLineHeightMetrics&);

  // TODO(kojii): Replace these floats with LayoutUnit.
  float ascent = 0;
  float descent = 0;
  float ascent_and_leading = 0;
  float descent_and_leading = 0;

  LayoutUnit LineHeight() const {
    return LayoutUnit(ascent_and_leading + descent_and_leading);
  }

 private:
  void Initialize(const FontMetrics&, FontBaseline, float line_height);
};

}  // namespace blink

#endif  // NGLineHeightMetrics_h
