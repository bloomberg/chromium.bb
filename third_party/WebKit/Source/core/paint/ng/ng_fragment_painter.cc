// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_fragment_painter.h"

#include "core/layout/LayoutTheme.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/ng/ng_paint_fragment.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

void NGFragmentPainter::PaintOutline(const PaintInfo& paint_info,
                                     const LayoutPoint& paint_offset) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));

  const ComputedStyle& style = paint_fragment_.Style();
  if (!style.HasOutline() || style.Visibility() != EVisibility::kVisible)
    return;

  // Only paint the focus ring by hand if the theme isn't able to draw the focus
  // ring.
  if (style.OutlineStyleIsAuto() &&
      !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(
          paint_fragment_.GetNode(), style)) {
    return;
  }

  Vector<LayoutRect> outline_rects;
  paint_fragment_.AddOutlineRects(&outline_rects, paint_offset);
  if (outline_rects.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, paint_fragment_, paint_info.phase))
    return;

  PaintOutlineRects(paint_info, outline_rects, style, paint_fragment_);
}

}  // namespace blink
