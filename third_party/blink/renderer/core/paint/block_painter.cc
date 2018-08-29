// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/block_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/line_box_list_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_info_with_offset.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/scoped_box_clipper.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"

namespace blink {

DISABLE_CFI_PERF
void BlockPainter::Paint(const PaintInfo& paint_info) {
  PaintInfoWithOffset paint_info_with_offset(layout_block_, paint_info);
  // We can't early return if there is no fragment to paint for this block,
  // because there may be overflowing children that exist in the painting
  // fragment. We also can't check ShouldPaint() in the case because we
  // don't have a meaningful paint offset. TODO(wangxianzhu): only paint
  // children if !adjustment.FragmentToPaint().
  if (paint_info_with_offset.FragmentToPaint() &&
      !ShouldPaint(paint_info_with_offset))
    return;

  auto paint_offset = paint_info_with_offset.PaintOffset();
  auto& local_paint_info = paint_info_with_offset.MutablePaintInfo();

  PaintPhase original_phase = local_paint_info.phase;

  if (original_phase == PaintPhase::kOutline) {
    local_paint_info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    local_paint_info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    layout_block_.PaintObject(local_paint_info, paint_offset);
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      local_paint_info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      original_phase != PaintPhase::kSelfOutlineOnly) {
    base::Optional<ScopedBoxClipper> box_clipper;
    if (local_paint_info.phase != PaintPhase::kMask)
      box_clipper.emplace(layout_block_, local_paint_info);
    layout_block_.PaintObject(local_paint_info, paint_offset);
  }

  // Carets are painted in the foreground phase, outside of the contents
  // properties block. Note that caret painting does not seem to correspond to
  // any painting order steps within the CSS spec.
  if (original_phase == PaintPhase::kForeground &&
      layout_block_.ShouldPaintCarets()) {
    PaintCarets(paint_info, paint_offset);
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    local_paint_info.phase = PaintPhase::kSelfOutlineOnly;
    layout_block_.PaintObject(local_paint_info, paint_offset);
  }

  // Our scrollbar widgets paint exactly when we tell them to, so that they work
  // properly with z-index. We paint after we painted the background/border, so
  // that the scrollbars will sit above the background/border.
  local_paint_info.phase = original_phase;
  PaintOverflowControlsIfNeeded(local_paint_info, paint_offset);
}

void BlockPainter::PaintOverflowControlsIfNeeded(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  if (layout_block_.HasOverflowClip() &&
      layout_block_.StyleRef().Visibility() == EVisibility::kVisible &&
      ShouldPaintSelfBlockBackground(paint_info.phase)) {
    ScrollableAreaPainter(*layout_block_.Layer()->GetScrollableArea())
        .PaintOverflowControls(paint_info, RoundedIntPoint(paint_offset),
                               false /* painting_overlay_controls */);
  }
}

void BlockPainter::PaintChildren(const PaintInfo& paint_info) {
  for (LayoutBox* child = layout_block_.FirstChildBox(); child;
       child = child->NextSiblingBox())
    PaintChild(*child, paint_info);
}

void BlockPainter::PaintChild(const LayoutBox& child,
                              const PaintInfo& paint_info) {
  if (!child.HasSelfPaintingLayer() && !child.IsFloating() &&
      !child.IsColumnSpanAll())
    child.Paint(paint_info);
}

void BlockPainter::PaintChildrenOfFlexibleBox(
    const LayoutFlexibleBox& layout_flexible_box,
    const PaintInfo& paint_info) {
  for (const LayoutBox* child = layout_flexible_box.GetOrderIterator().First();
       child; child = layout_flexible_box.GetOrderIterator().Next()) {
    BlockPainter(layout_flexible_box)
        .PaintAllChildPhasesAtomically(*child, paint_info);
  }
}

void BlockPainter::PaintAllChildPhasesAtomically(const LayoutBox& child,
                                                 const PaintInfo& paint_info) {
  if (!child.HasSelfPaintingLayer() && !child.IsFloating())
    ObjectPainter(child).PaintAllPhasesAtomically(paint_info);
}

void BlockPainter::PaintInlineBox(const InlineBox& inline_box,
                                  const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection)
    return;

  // Text clips are painted only for the direct inline children of the object
  // that has a text clip style on it, not block children.
  DCHECK(paint_info.phase != PaintPhase::kTextClip);

  ObjectPainter(
      *LineLayoutAPIShim::ConstLayoutObjectFrom(inline_box.GetLineLayoutItem()))
      .PaintAllPhasesAtomically(paint_info);
}

void BlockPainter::PaintScrollHitTestDisplayItem(const PaintInfo& paint_info) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());

  // Scroll hit test display items are only needed for compositing. This flag is
  // used for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  // The scroll hit test layer is in the unscrolled and unclipped space so the
  // scroll hit test layer can be enlarged beyond the clip. This will let us fix
  // crbug.com/753124 in the future where the scrolling element's border is hit
  // test differently if composited.

  const auto* fragment = paint_info.FragmentToPaint(layout_block_);
  const auto* properties = fragment ? fragment->PaintProperties() : nullptr;

  // If there is an associated scroll node, emit a scroll hit test display item.
  if (properties && properties->Scroll()) {
    DCHECK(properties->ScrollTranslation());
    // The local border box properties are used instead of the contents
    // properties so that the scroll hit test is not clipped or scrolled.
    ScopedPaintChunkProperties scroll_hit_test_properties(
        paint_info.context.GetPaintController(),
        fragment->LocalBorderBoxProperties(), layout_block_,
        DisplayItem::kScrollHitTest);
    ScrollHitTestDisplayItem::Record(paint_info.context, layout_block_,
                                     *properties->ScrollTranslation());
  }
}

// TODO(pdr): Non-blocks also need to paint the hit test display item. Move this
// to a more central place such as BoxPainter.
void BlockPainter::RecordHitTestData(const PaintInfo& paint_info,
                                     const LayoutPoint& paint_offset) {
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  auto touch_action = layout_block_.EffectiveWhitelistedTouchAction();
  if (touch_action == TouchAction::kTouchActionAuto)
    return;

  // TODO(pdr): If we are painting the background into the scrolling contents
  // layer, we need to use the overflow rect instead of the border box rect. We
  // may want to move the call to RecordTouchActionRect into
  // BoxPainter::PaintBoxDecorationBackgroundWithRect and share the logic
  // the background painting code already uses.
  auto rect = layout_block_.BorderBoxRect();
  rect.MoveBy(paint_offset);
  HitTestData::RecordTouchActionRect(paint_info.context, layout_block_,
                                     TouchActionRect(rect, touch_action));
}

DISABLE_CFI_PERF
void BlockPainter::PaintObject(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  const PaintPhase paint_phase = paint_info.phase;
  // This function implements some of the painting order algorithm (described
  // within the description of stacking context, here
  // https://www.w3.org/TR/css-position-3/#det-stacking-context). References are
  // made below to the step numbers described in that document.

  // If this block has been truncated, early-out here, because it will not be
  // displayed. A truncated block occurs when text-overflow: ellipsis is set on
  // a block, and there is not enough room to display all elements. The elements
  // that don't get shown are "Truncated".
  if (layout_block_.IsTruncated())
    return;

  // If we're *printing* the foreground, paint the URL.
  if (paint_phase == PaintPhase::kForeground && paint_info.IsPrinting()) {
    ObjectPainter(layout_block_)
        .AddPDFURLRectIfNeeded(paint_info, paint_offset);
  }

  // If we're painting our background (either 1. kBlockBackground - background
  // of the current object and non-self-painting descendants, or 2.
  // kSelfBlockBackgroundOnly -  Paint background of the current object only),
  // paint those now. This is steps #1, 2, and 4 of the CSS spec (see above).
  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    // Paint the background if we're visible and this block has a box decoration
    // (background, border, appearance, or box shadow).
    if (layout_block_.StyleRef().Visibility() == EVisibility::kVisible &&
        layout_block_.HasBoxDecorationBackground()) {
      layout_block_.PaintBoxDecorationBackground(paint_info, paint_offset);
    }
    if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled())
      RecordHitTestData(paint_info, paint_offset);
    // Record the scroll hit test after the background so background squashing
    // is not affected. Hit test order would be equivalent if this were
    // immediately before the background.
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
      PaintScrollHitTestDisplayItem(paint_info);
  }

  // If we're in any phase except *just* the self (outline or background) or a
  // mask, paint children now. This is step #5, 7, 8, and 9 of the CSS spec (see
  // above).
  if (paint_phase != PaintPhase::kSelfOutlineOnly &&
      paint_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      paint_phase != PaintPhase::kMask) {
    // Handle scrolling translation.
    base::Optional<PaintInfo> scrolled_paint_info;
    if (const auto* fragment = paint_info.FragmentToPaint(layout_block_)) {
      const auto* object_properties = fragment->PaintProperties();
      auto* scroll_translation =
          object_properties ? object_properties->ScrollTranslation() : nullptr;
      if (scroll_translation) {
        scrolled_paint_info.emplace(paint_info);
        if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
          scrolled_paint_info->UpdateCullRectForScrollingContents(
              EnclosingIntRect(layout_block_.OverflowClipRect(paint_offset)),
              scroll_translation->Matrix().ToAffineTransform());
        } else {
          scrolled_paint_info->UpdateCullRect(
              scroll_translation->Matrix().ToAffineTransform());
        }
      }
    }
    const PaintInfo& contents_paint_info =
        scrolled_paint_info ? *scrolled_paint_info : paint_info;

    // Actually paint the contents.
    if (layout_block_.IsLayoutBlockFlow()) {
      // All floating descendants will be LayoutBlockFlow objects, and will get
      // painted here. That is step #5 of the CSS spec (see above).
      PaintBlockFlowContents(contents_paint_info, paint_offset);
    } else {
      PaintContents(contents_paint_info, paint_offset);
    }
  }

  // If we're painting the outline, paint it now. This is step #10 of the CSS
  // spec (see above).
  if (ShouldPaintSelfOutline(paint_phase))
    ObjectPainter(layout_block_).PaintOutline(paint_info, paint_offset);

  // If we're painting a visible mask, paint it now. (This does not correspond
  // to any painting order steps within the CSS spec.)
  if (paint_phase == PaintPhase::kMask &&
      layout_block_.StyleRef().Visibility() == EVisibility::kVisible) {
    layout_block_.PaintMask(paint_info, paint_offset);
  }
}

void BlockPainter::PaintBlockFlowContents(const PaintInfo& paint_info,
                                          const LayoutPoint& paint_offset) {
  DCHECK(layout_block_.IsLayoutBlockFlow());
  if (layout_block_.IsLayoutView() ||
      !paint_info.SuppressPaintingDescendants()) {
    if (!layout_block_.ChildrenInline()) {
      PaintContents(paint_info, paint_offset);
    } else if (ShouldPaintDescendantOutlines(paint_info.phase)) {
      ObjectPainter(layout_block_).PaintInlineChildrenOutlines(paint_info);
    } else {
      LineBoxListPainter(ToLayoutBlockFlow(layout_block_).LineBoxes())
          .Paint(layout_block_, paint_info, paint_offset);
    }
  }

  // If we don't have any floats to paint, or we're in the wrong paint phase,
  // then we're done for now.
  auto* floating_objects =
      ToLayoutBlockFlow(layout_block_).GetFloatingObjects();
  const PaintPhase paint_phase = paint_info.phase;
  if (!floating_objects || !(paint_phase == PaintPhase::kFloat ||
                             paint_phase == PaintPhase::kSelection ||
                             paint_phase == PaintPhase::kTextClip)) {
    return;
  }

  // If we're painting floats (not selections or textclips), change
  // the paint phase to foreground.
  PaintInfo float_paint_info(paint_info);
  if (paint_info.phase == PaintPhase::kFloat)
    float_paint_info.phase = PaintPhase::kForeground;

  // Paint all floats.
  for (const auto& floating_object : floating_objects->Set()) {
    if (!floating_object->ShouldPaint())
      continue;
    const LayoutBox* floating_layout_object =
        floating_object->GetLayoutObject();
    // TODO(wangxianzhu): Should this be a DCHECK?
    if (floating_layout_object->HasSelfPaintingLayer())
      continue;
    ObjectPainter(*floating_layout_object)
        .PaintAllPhasesAtomically(float_paint_info);
  }
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
bool BlockPainter::ShouldPaint(
    const PaintInfoWithOffset& paint_info_with_offset) const {
  LayoutRect overflow_rect;
  if (paint_info_with_offset.GetPaintInfo().IsPrinting() &&
      layout_block_.IsAnonymousBlock() && layout_block_.ChildrenInline()) {
    // For case <a href="..."><div>...</div></a>, when layout_block_ is the
    // anonymous container of <a>, the anonymous container's visual overflow is
    // empty, but we need to continue painting to output <a>'s PDF URL rect
    // which covers the continuations, as if we included <a>'s PDF URL rect into
    // layout_block_'s visual overflow.
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

  return paint_info_with_offset.LocalRectIntersectsCullRect(overflow_rect);
}

void BlockPainter::PaintContents(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  DCHECK(!layout_block_.ChildrenInline());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  layout_block_.PaintChildren(paint_info_for_descendants, paint_offset);
}

}  // namespace blink
