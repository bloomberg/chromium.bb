// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGRootInlineBoxPainter.h"

#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/svg/line/SVGInlineFlowBox.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "core/layout/svg/line/SVGRootInlineBox.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGInlineFlowBoxPainter.h"
#include "core/paint/SVGInlineTextBoxPainter.h"
#include "core/paint/SVGPaintContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void SVGRootInlineBoxPainter::Paint(const PaintInfo& paint_info,
                                    const LayoutPoint& paint_offset) {
  DCHECK(paint_info.phase == PaintPhase::kForeground ||
         paint_info.phase == PaintPhase::kSelection);

  bool has_selection =
      !paint_info.IsPrinting() &&
      svg_root_inline_box_.GetSelectionState() != SelectionState::kNone;

  PaintInfo paint_info_before_filtering(paint_info);
  if (has_selection && !DrawingRecorder::UseCachedDrawingIfPossible(
                           paint_info_before_filtering.context,
                           *LineLayoutAPIShim::ConstLayoutObjectFrom(
                               svg_root_inline_box_.GetLineLayoutItem()),
                           paint_info_before_filtering.phase)) {
    DrawingRecorder recorder(paint_info_before_filtering.context,
                             *LineLayoutAPIShim::ConstLayoutObjectFrom(
                                 svg_root_inline_box_.GetLineLayoutItem()),
                             paint_info_before_filtering.phase,
                             paint_info_before_filtering.GetCullRect().rect_);
    for (InlineBox* child = svg_root_inline_box_.FirstChild(); child;
         child = child->NextOnLine()) {
      if (child->IsSVGInlineTextBox())
        SVGInlineTextBoxPainter(*ToSVGInlineTextBox(child))
            .PaintSelectionBackground(paint_info_before_filtering);
      else if (child->IsSVGInlineFlowBox())
        SVGInlineFlowBoxPainter(*ToSVGInlineFlowBox(child))
            .PaintSelectionBackground(paint_info_before_filtering);
    }
  }

  SVGPaintContext paint_context(*LineLayoutAPIShim::ConstLayoutObjectFrom(
                                    svg_root_inline_box_.GetLineLayoutItem()),
                                paint_info_before_filtering);
  if (paint_context.ApplyClipMaskAndFilterIfNecessary()) {
    for (InlineBox* child = svg_root_inline_box_.FirstChild(); child;
         child = child->NextOnLine())
      child->Paint(paint_context.GetPaintInfo(), paint_offset, LayoutUnit(),
                   LayoutUnit());
  }
}

}  // namespace blink
