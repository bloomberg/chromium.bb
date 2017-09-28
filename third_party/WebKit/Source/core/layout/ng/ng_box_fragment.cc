// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box_fragment.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGLogicalSize NGBoxFragment::OverflowSize() const {
  const auto& physical_fragment = ToNGPhysicalBoxFragment(physical_fragment_);
  return physical_fragment.OverflowSize().ConvertToLogical(WritingMode());
}

NGLineHeightMetrics NGBoxFragment::BaselineMetrics(
    const NGBaselineRequest& request) const {
  const auto& physical_fragment = ToNGPhysicalBoxFragment(physical_fragment_);

  LayoutBox* layout_box = ToLayoutBox(physical_fragment_.GetLayoutObject());

  // Find the baseline from the computed results.
  if (const NGBaseline* baseline = physical_fragment.Baseline(request)) {
    LayoutUnit ascent = baseline->offset;
    LayoutUnit descent = BlockSize() - ascent;

    // For replaced elements, inline-block elements, and inline-table
    // elements, the height is the height of their margin box.
    // https://drafts.csswg.org/css2/visudet.html#line-height
    if (layout_box->IsAtomicInlineLevel()) {
      ascent += layout_box->MarginOver();
      descent += layout_box->MarginUnder();
    }

    return NGLineHeightMetrics(ascent, descent);
  }

  // The baseline type was not found. This is either this box should synthesize
  // box-baseline without propagating from children, or caller forgot to add
  // baseline requests to constraint space when it called Layout().
  LayoutUnit block_size = BlockSize();

  // If atomic inline, use the margin box. See above.
  if (layout_box->IsAtomicInlineLevel())
    block_size += layout_box->MarginLogicalHeight();

  if (request.baseline_type == kAlphabeticBaseline)
    return NGLineHeightMetrics(block_size, LayoutUnit());
  return NGLineHeightMetrics(block_size - block_size / 2, block_size / 2);
}

}  // namespace blink
