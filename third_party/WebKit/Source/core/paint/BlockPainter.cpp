// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockPainter.h"

#include "core/editing/DragCaret.h"
#include "core/editing/FrameSelection.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/page/Page.h"
#include "core/paint/BlockFlowPainter.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ScrollRecorder.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/wtf/Optional.h"

namespace blink {

DISABLE_CFI_PERF
void BlockPainter::Paint(const PaintInfo& paint_info,
                         const LayoutPoint& paint_offset) {
  ObjectPainter(layout_block_).CheckPaintOffset(paint_info, paint_offset);
  LayoutPoint adjusted_paint_offset = paint_offset + layout_block_.Location();
  if (!IntersectsPaintRect(paint_info, adjusted_paint_offset))
    return;

  PaintInfo local_paint_info(paint_info);
  PaintPhase original_phase = local_paint_info.phase;

  // There are some cases where not all clipped visual overflow is accounted
  // for.
  // FIXME: reduce the number of such cases.
  ContentsClipBehavior contents_clip_behavior = kForceContentsClip;
  if (layout_block_.ShouldClipOverflow() && !layout_block_.HasControlClip() &&
      !layout_block_.ShouldPaintCarets())
    contents_clip_behavior = kSkipContentsClipIfPossible;

  if (original_phase == kPaintPhaseOutline) {
    local_paint_info.phase = kPaintPhaseDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    local_paint_info.phase = kPaintPhaseSelfBlockBackgroundOnly;
    layout_block_.PaintObject(local_paint_info, adjusted_paint_offset);
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      local_paint_info.phase = kPaintPhaseDescendantBlockBackgroundsOnly;
  }

  if (original_phase != kPaintPhaseSelfBlockBackgroundOnly &&
      original_phase != kPaintPhaseSelfOutlineOnly) {
    BoxClipper box_clipper(layout_block_, local_paint_info,
                           adjusted_paint_offset, contents_clip_behavior);
    layout_block_.PaintObject(local_paint_info, adjusted_paint_offset);
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    local_paint_info.phase = kPaintPhaseSelfOutlineOnly;
    layout_block_.PaintObject(local_paint_info, adjusted_paint_offset);
  }

  // Our scrollbar widgets paint exactly when we tell them to, so that they work
  // properly with z-index. We paint after we painted the background/border, so
  // that the scrollbars will sit above the background/border.
  local_paint_info.phase = original_phase;
  PaintOverflowControlsIfNeeded(local_paint_info, adjusted_paint_offset);
}

void BlockPainter::PaintOverflowControlsIfNeeded(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  if (layout_block_.HasOverflowClip() &&
      layout_block_.Style()->Visibility() == EVisibility::kVisible &&
      ShouldPaintSelfBlockBackground(paint_info.phase) &&
      !paint_info.PaintRootBackgroundOnly()) {
    Optional<ClipRecorder> clip_recorder;
    if (!layout_block_.Layer()->IsSelfPaintingLayer()) {
      LayoutRect clip_rect = layout_block_.BorderBoxRect();
      clip_rect.MoveBy(paint_offset);
      clip_recorder.emplace(paint_info.context, layout_block_,
                            DisplayItem::kClipScrollbarsToBoxBounds,
                            PixelSnappedIntRect(clip_rect));
    }
    ScrollableAreaPainter(*layout_block_.Layer()->GetScrollableArea())
        .PaintOverflowControls(
            paint_info.context, RoundedIntPoint(paint_offset),
            paint_info.GetCullRect(), false /* paintingOverlayControls */);
  }
}

void BlockPainter::PaintChildren(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  for (LayoutBox* child = layout_block_.FirstChildBox(); child;
       child = child->NextSiblingBox())
    PaintChild(*child, paint_info, paint_offset);
}

void BlockPainter::PaintChild(const LayoutBox& child,
                              const PaintInfo& paint_info,
                              const LayoutPoint& paint_offset) {
  LayoutPoint child_point =
      layout_block_.FlipForWritingModeForChild(&child, paint_offset);
  if (!child.HasSelfPaintingLayer() && !child.IsFloating() &&
      !child.IsColumnSpanAll())
    child.Paint(paint_info, child_point);
}

void BlockPainter::PaintChildrenOfFlexibleBox(
    const LayoutFlexibleBox& layout_flexible_box,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  for (const LayoutBox* child = layout_flexible_box.GetOrderIterator().First();
       child; child = layout_flexible_box.GetOrderIterator().Next())
    BlockPainter(layout_flexible_box)
        .PaintAllChildPhasesAtomically(*child, paint_info, paint_offset);
}

void BlockPainter::PaintAllChildPhasesAtomically(
    const LayoutBox& child,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  LayoutPoint child_point =
      layout_block_.FlipForWritingModeForChild(&child, paint_offset);
  if (!child.HasSelfPaintingLayer() && !child.IsFloating())
    ObjectPainter(child).PaintAllPhasesAtomically(paint_info, child_point);
}

void BlockPainter::PaintInlineBox(const InlineBox& inline_box,
                                  const PaintInfo& paint_info,
                                  const LayoutPoint& paint_offset) {
  if (paint_info.phase != kPaintPhaseForeground &&
      paint_info.phase != kPaintPhaseSelection)
    return;

  // Text clips are painted only for the direct inline children of the object
  // that has a text clip style on it, not block children.
  DCHECK(paint_info.phase != kPaintPhaseTextClip);

  LayoutPoint child_point = paint_offset;
  if (inline_box.Parent()
          ->GetLineLayoutItem()
          .Style()
          ->IsFlippedBlocksWritingMode()) {
    // Faster than calling containingBlock().
    child_point =
        LineLayoutAPIShim::LayoutObjectFrom(inline_box.GetLineLayoutItem())
            ->ContainingBlock()
            ->FlipForWritingModeForChild(
                ToLayoutBox(LineLayoutAPIShim::LayoutObjectFrom(
                    inline_box.GetLineLayoutItem())),
                child_point);
  }

  ObjectPainter(
      *LineLayoutAPIShim::ConstLayoutObjectFrom(inline_box.GetLineLayoutItem()))
      .PaintAllPhasesAtomically(paint_info, child_point);
}

DISABLE_CFI_PERF
void BlockPainter::PaintObject(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  if (layout_block_.IsTruncated())
    return;

  const PaintPhase paint_phase = paint_info.phase;

  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    if (layout_block_.Style()->Visibility() == EVisibility::kVisible &&
        layout_block_.HasBoxDecorationBackground())
      layout_block_.PaintBoxDecorationBackground(paint_info, paint_offset);
    // We're done. We don't bother painting any children.
    if (paint_phase == kPaintPhaseSelfBlockBackgroundOnly)
      return;
  }

  if (paint_info.PaintRootBackgroundOnly())
    return;

  if (paint_phase == kPaintPhaseMask &&
      layout_block_.Style()->Visibility() == EVisibility::kVisible) {
    layout_block_.PaintMask(paint_info, paint_offset);
    return;
  }

  if (paint_phase == kPaintPhaseClippingMask &&
      layout_block_.Style()->Visibility() == EVisibility::kVisible) {
    BoxPainter(layout_block_).PaintClippingMask(paint_info, paint_offset);
    return;
  }

  if (paint_phase == kPaintPhaseForeground && paint_info.IsPrinting())
    ObjectPainter(layout_block_)
        .AddPDFURLRectIfNeeded(paint_info, paint_offset);

  if (paint_phase != kPaintPhaseSelfOutlineOnly) {
    Optional<ScopedPaintChunkProperties> scoped_scroll_property;
    Optional<ScrollRecorder> scroll_recorder;
    Optional<PaintInfo> scrolled_paint_info;
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      const auto* object_properties = layout_block_.PaintProperties();
      auto* scroll_translation =
          object_properties ? object_properties->ScrollTranslation() : nullptr;
      if (scroll_translation) {
        PaintChunkProperties properties(paint_info.context.GetPaintController()
                                            .CurrentPaintChunkProperties());
        properties.property_tree_state.SetTransform(scroll_translation);
        scoped_scroll_property.emplace(
            paint_info.context.GetPaintController(), layout_block_,
            DisplayItem::PaintPhaseToDrawingType(paint_phase), properties);
        scrolled_paint_info.emplace(paint_info);
        scrolled_paint_info->UpdateCullRectForScrollingContents(
            EnclosingIntRect(layout_block_.OverflowClipRect(paint_offset)),
            scroll_translation->Matrix().ToAffineTransform());
      }
    } else if (layout_block_.HasOverflowClip()) {
      IntSize scroll_offset = layout_block_.ScrolledContentOffset();
      if (layout_block_.Layer()->ScrollsOverflow() || !scroll_offset.IsZero()) {
        scroll_recorder.emplace(paint_info.context, layout_block_, paint_phase,
                                scroll_offset);
        scrolled_paint_info.emplace(paint_info);
        AffineTransform transform;
        transform.Translate(-scroll_offset.Width(), -scroll_offset.Height());
        scrolled_paint_info->UpdateCullRect(transform);
      }
    }

    const PaintInfo& contents_paint_info =
        scrolled_paint_info ? *scrolled_paint_info : paint_info;

    if (layout_block_.IsLayoutBlockFlow()) {
      BlockFlowPainter block_flow_painter(ToLayoutBlockFlow(layout_block_));
      block_flow_painter.PaintContents(contents_paint_info, paint_offset);
      if (paint_phase == kPaintPhaseFloat ||
          paint_phase == kPaintPhaseSelection ||
          paint_phase == kPaintPhaseTextClip)
        block_flow_painter.PaintFloats(contents_paint_info, paint_offset);
    } else {
      PaintContents(contents_paint_info, paint_offset);
    }
  }

  if (ShouldPaintSelfOutline(paint_phase))
    ObjectPainter(layout_block_).PaintOutline(paint_info, paint_offset);

  // If the caret's node's layout object's containing block is this block, and
  // the paint action is PaintPhaseForeground, then paint the caret.
  if (paint_phase == kPaintPhaseForeground && layout_block_.ShouldPaintCarets())
    PaintCarets(paint_info, paint_offset);
}

void BlockPainter::PaintCarets(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  LocalFrame* frame = layout_block_.GetFrame();

  if (layout_block_.ShouldPaintCursorCaret())
    frame->Selection().PaintCaret(paint_info.context, paint_offset);

  if (layout_block_.ShouldPaintDragCaret()) {
    frame->GetPage()->GetDragCaret().PaintDragCaret(frame, paint_info.context,
                                                    paint_offset);
  }
}

DISABLE_CFI_PERF
bool BlockPainter::IntersectsPaintRect(
    const PaintInfo& paint_info,
    const LayoutPoint& adjusted_paint_offset) const {
  LayoutRect overflow_rect;
  if (paint_info.IsPrinting() && layout_block_.IsAnonymousBlock() &&
      layout_block_.ChildrenInline()) {
    // For case <a href="..."><div>...</div></a>, when m_layoutBlock is the
    // anonymous container of <a>, the anonymous container's visual overflow is
    // empty, but we need to continue painting to output <a>'s PDF URL rect
    // which covers the continuations, as if we included <a>'s PDF URL rect into
    // m_layoutBlock's visual overflow.
    Vector<LayoutRect> rects;
    layout_block_.AddElementVisualOverflowRects(rects, LayoutPoint());
    overflow_rect = UnionRect(rects);
  }
  overflow_rect.Unite(layout_block_.VisualOverflowRect());

  bool uses_composited_scrolling = layout_block_.HasOverflowModel() &&
                                   layout_block_.UsesCompositedScrolling();

  if (uses_composited_scrolling) {
    LayoutRect layout_overflow_rect = layout_block_.LayoutOverflowRect();
    overflow_rect.Unite(layout_overflow_rect);
  }
  layout_block_.FlipForWritingMode(overflow_rect);

  // Scrolling is applied in physical space, which is why it is after the flip
  // above.
  if (uses_composited_scrolling) {
    overflow_rect.Move(-layout_block_.ScrolledContentOffset());
  }

  overflow_rect.MoveBy(adjusted_paint_offset);
  return paint_info.GetCullRect().IntersectsCullRect(overflow_rect);
}

void BlockPainter::PaintContents(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  DCHECK(!layout_block_.ChildrenInline());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  layout_block_.PaintChildren(paint_info_for_descendants, paint_offset);
}

}  // namespace blink
