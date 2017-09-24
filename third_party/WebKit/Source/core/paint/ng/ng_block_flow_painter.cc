// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_block_flow_painter.h"

#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/ng/ng_box_fragment_painter.h"

namespace blink {

void NGBlockFlowPainter::PaintContents(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset) {
  RefPtr<const NGPhysicalBoxFragment> box_fragment = block_.RootFragment();
  if (box_fragment)
    PaintBoxFragment(*box_fragment, paint_info, paint_offset);
}

void NGBlockFlowPainter::PaintBoxFragment(const NGPhysicalBoxFragment& fragment,
                                          const PaintInfo& paint_info,
                                          const LayoutPoint& paint_offset) {
  PaintInfo ng_paint_info(paint_info);
  NGBoxFragmentPainter(fragment).Paint(ng_paint_info, paint_offset);
}

}  // namespace blink
