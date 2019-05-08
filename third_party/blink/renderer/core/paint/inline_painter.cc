// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_painter.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/paint/line_box_list_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"

namespace blink {

void InlinePainter::Paint(const PaintInfo& paint_info) {
  base::Optional<ScopedPaintTimingDetectorBlockPaintHook>
      scoped_paint_timing_detector_block_paint_hook;
  if (RuntimeEnabledFeatures::FirstContentfulPaintPlusPlusEnabled())
    scoped_paint_timing_detector_block_paint_hook.emplace(layout_inline_);

  ScopedPaintState paint_state(layout_inline_, paint_info);
  auto paint_offset = paint_state.PaintOffset();
  const auto& local_paint_info = paint_state.GetPaintInfo();

  if (local_paint_info.phase == PaintPhase::kForeground &&
      local_paint_info.IsPrinting()) {
    ObjectPainter(layout_inline_)
        .AddPDFURLRectIfNeeded(local_paint_info, paint_offset);
  }

  if (layout_inline_.IsInLayoutNGInlineFormattingContext()) {
    for (const NGPaintFragment* fragment :
         NGPaintFragment::InlineFragmentsFor(&layout_inline_)) {
      NGInlineBoxFragmentPainter(*fragment).Paint(
          paint_info, paint_offset + (fragment->InlineOffsetToContainerBox() -
                                      fragment->Offset())
                                         .ToLayoutPoint());
    }
    return;
  }

  if (ShouldPaintSelfOutline(local_paint_info.phase) ||
      ShouldPaintDescendantOutlines(local_paint_info.phase)) {
    ObjectPainter painter(layout_inline_);
    if (ShouldPaintDescendantOutlines(local_paint_info.phase))
      painter.PaintInlineChildrenOutlines(local_paint_info);
    if (ShouldPaintSelfOutline(local_paint_info.phase) &&
        !layout_inline_.IsElementContinuation()) {
      painter.PaintOutline(local_paint_info, paint_offset);
    }
    return;
  }

  LineBoxListPainter(*layout_inline_.LineBoxes())
      .Paint(layout_inline_, local_paint_info, paint_offset);
}

}  // namespace blink
