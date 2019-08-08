// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scrollbar_painter.h"

#include "third_party/blink/renderer/core/layout/layout_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_scrollbar_part.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

void ScrollbarPainter::PaintPart(GraphicsContext& graphics_context,
                                 ScrollbarPart part_type,
                                 const IntRect& rect) {
  const LayoutScrollbarPart* part_layout_object =
      layout_scrollbar_->GetPart(part_type);
  if (!part_layout_object)
    return;
  PaintIntoRect(*part_layout_object, graphics_context,
                PhysicalOffset(layout_scrollbar_->Location()),
                PhysicalRect(rect), layout_scrollbar_.Get());
}

void ScrollbarPainter::PaintIntoRect(
    const LayoutScrollbarPart& layout_scrollbar_part,
    GraphicsContext& graphics_context,
    const PhysicalOffset& paint_offset,
    const PhysicalRect& rect,
    const LayoutScrollbar* scrollbar) {
  // Make sure our dimensions match the rect.
  // TODO(crbug.com/856802): Setting these is a bad layering violation!
  // Move these into layout stage.
  const_cast<LayoutScrollbarPart&>(layout_scrollbar_part)
      .SetLocation((rect.offset - paint_offset).ToLayoutPoint());
  const_cast<LayoutScrollbarPart&>(layout_scrollbar_part)
      .SetWidth(rect.size.width);
  const_cast<LayoutScrollbarPart&>(layout_scrollbar_part)
      .SetHeight(rect.size.height);
  // TODO(crbug.com/856802): Move this into PaintPropertyTreeBuilder.
  layout_scrollbar_part.GetMutableForPainting().FirstFragment().SetPaintOffset(
      (scrollbar ? PhysicalOffset(scrollbar->Location()) : PhysicalOffset()) +
      layout_scrollbar_part.PhysicalLocation());

  PaintInfo paint_info(graphics_context, PixelSnappedIntRect(rect),
                       PaintPhase::kForeground, kGlobalPaintNormalPhase,
                       kPaintLayerNoFlag);
  ObjectPainter(layout_scrollbar_part).PaintAllPhasesAtomically(paint_info);
}

}  // namespace blink
