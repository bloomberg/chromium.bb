// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_block_flow_painter.h"

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"

namespace blink {

void NGBlockFlowPainter::Paint(const PaintInfo& paint_info) {
  const NGPaintFragment* paint_fragment = block_.PaintFragment();
  DCHECK(paint_fragment);
  PaintBoxFragment(*paint_fragment, paint_info);
}

void NGBlockFlowPainter::PaintBoxFragment(const NGPaintFragment& fragment,
                                          const PaintInfo& paint_info) {
  PaintInfo ng_paint_info(paint_info);
  NGBoxFragmentPainter(fragment).Paint(ng_paint_info);
}

bool NGBlockFlowPainter::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& physical_offset,
    HitTestAction action) {
  if (const NGPaintFragment* paint_fragment = block_.PaintFragment()) {
    return NGBoxFragmentPainter(*paint_fragment)
        .NodeAtPoint(result, location_in_container, physical_offset, action);
  }
  return false;
}

}  // namespace blink
