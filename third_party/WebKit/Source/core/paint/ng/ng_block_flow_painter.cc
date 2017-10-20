// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_block_flow_painter.h"

#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/ng/ng_box_fragment_painter.h"
#include "core/paint/ng/ng_paint_fragment.h"

namespace blink {

void NGBlockFlowPainter::Paint(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  const NGPaintFragment* paint_fragment = block_.PaintFragment();
  DCHECK(paint_fragment);
  PaintBoxFragment(*paint_fragment, paint_info, paint_offset);
}

void NGBlockFlowPainter::PaintBoxFragment(const NGPaintFragment& fragment,
                                          const PaintInfo& paint_info,
                                          const LayoutPoint& paint_offset) {
  PaintInfo ng_paint_info(paint_info);
  NGBoxFragmentPainter(fragment).Paint(ng_paint_info, paint_offset);
}

bool NGBlockFlowPainter::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  if (const NGPaintFragment* paint_fragment = block_.PaintFragment()) {
    return NGBoxFragmentPainter(*paint_fragment)
        .NodeAtPoint(result, location_in_container, accumulated_offset, action);
  }
  return false;
}

}  // namespace blink
